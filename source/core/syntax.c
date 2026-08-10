#include "gcalc/syntax.h"

#include <string.h>

typedef struct SyntaxParser {
    const char *source;
    s16 length;
    s16 position;
    CalcSyntaxAst *ast;
    u8 error;
    u8 commaSeparates;
} SyntaxParser;

static char lowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (char)(value + ('a' - 'A'));
    return value;
}

static u8 equalText(const char *left, const char *right)
{
    while (*left && *right)
    {
        if (lowerAscii(*left) != lowerAscii(*right))
            return 0;
        ++left;
        ++right;
    }
    return (u8)(*left == '\0' && *right == '\0');
}

static u8 startsText(const char *source, const char *word)
{
    while (*word)
    {
        if (lowerAscii(*source) != lowerAscii(*word))
            return 0;
        ++source;
        ++word;
    }
    return 1;
}

static u8 isSpaceChar(char value)
{
    return (u8)(value == ' ' || value == '\t' || value == '\r' ||
                value == '\n');
}

static u8 isDigitChar(char value)
{
    return (u8)(value >= '0' && value <= '9');
}

static u8 isAlphaChar(char value)
{
    value = lowerAscii(value);
    return (u8)(value >= 'a' && value <= 'z');
}

static u8 isIdentifierChar(char value)
{
    return (u8)(isAlphaChar(value) || isDigitChar(value) || value == '_' ||
                value == '#');
}

static void skipSpaces(SyntaxParser *parser)
{
    while (parser->position < parser->length &&
           isSpaceChar(parser->source[parser->position]))
        ++parser->position;
}

static s16 makeNode(SyntaxParser *parser, CalcSyntaxNodeKind kind,
                    s16 start, s16 end)
{
    CalcSyntaxNode *node;
    s16 index;

    if (parser->error || parser->ast->count >= CALC_SYNTAX_MAX_NODES)
    {
        parser->error = CALC_ERR_SYNTAX;
        return -1;
    }
    index = (s16)parser->ast->count++;
    node = &parser->ast->nodes[index];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    node->start = start;
    node->end = end;
    node->left = -1;
    node->right = -1;
    {
        u8 i;
        for (i = 0; i < CALC_SYNTAX_MAX_ARGS; ++i)
            node->args[i] = -1;
    }
    return index;
}

static void copySpan(char *destination, u8 capacity, const char *source,
                     s16 start, s16 end)
{
    u8 count = 0;

    if (capacity == 0)
        return;
    while (start < end && count + 1 < capacity)
        destination[count++] = source[start++];
    destination[count] = '\0';
}

static s16 makeBinary(SyntaxParser *parser, CalcSyntaxNodeKind kind,
                      char op, const char *text, s16 left, s16 right)
{
    CalcSyntaxNode *node;
    s16 index;

    if (left < 0 || right < 0)
    {
        parser->error = CALC_ERR_SYNTAX;
        return -1;
    }
    index = makeNode(parser, kind, parser->ast->nodes[left].start,
                     parser->ast->nodes[right].end);
    if (index < 0)
        return -1;
    node = &parser->ast->nodes[index];
    node->op = op;
    node->left = left;
    node->right = right;
    if (text != NULL)
    {
        u8 i = 0;
        while (text[i] && i + 1 < CALC_SYNTAX_TEXT)
        {
            node->text[i] = text[i];
            ++i;
        }
        node->text[i] = '\0';
    }
    return index;
}

static u8 unaryFunctionName(const char *name)
{
    static const char *const names[] = {
        "sin", "cos", "tan", "asin", "acos", "atan",
        "sin^-1", "cos^-1", "tan^-1", "sinh", "cosh", "tanh",
        "asinh", "acosh", "atanh", "sqrt", "cbrt", "ln", "log",
        "log10", "lg", "exp", "fac", "abs", "floor", "ceil",
        "trunc", "int", "intg", "frac", "round", "sec", "csc",
        "cot", "sech", "csch", "coth", "recip", "sqr", "sign",
        "deg", "rad", "grad", "pow10"
    };
    u8 i;

    for (i = 0; i < (u8)(sizeof(names) / sizeof(names[0])); ++i)
    {
        if (equalText(name, names[i]))
            return 1;
    }
    return 0;
}

static u8 topLevelSemicolon(const SyntaxParser *parser, s16 openPosition)
{
    s16 position = (s16)(openPosition + 1);
    s16 depth = 0;

    while (position < parser->length)
    {
        char value = parser->source[position++];
        if (value == '(')
            ++depth;
        else if (value == ')')
        {
            if (depth == 0)
                return 0;
            --depth;
        }
        else if (value == ';' && depth == 0)
            return 1;
    }
    return 0;
}

static s16 parseRelation(SyntaxParser *parser);

static s16 parseNumber(SyntaxParser *parser)
{
    s16 start = parser->position;
    s16 index;
    u8 sawDigit = 0;
    u8 sawDecimal = 0;

    while (parser->position < parser->length &&
           isDigitChar(parser->source[parser->position]))
    {
        sawDigit = 1;
        ++parser->position;
    }
    if (parser->position < parser->length &&
        (parser->source[parser->position] == '.' ||
         (parser->source[parser->position] == ',' &&
          !parser->commaSeparates)))
    {
        sawDecimal = 1;
        ++parser->position;
        while (parser->position < parser->length &&
               isDigitChar(parser->source[parser->position]))
        {
            sawDigit = 1;
            ++parser->position;
        }
    }
    if (!sawDigit)
    {
        parser->error = CALC_ERR_SYNTAX;
        return -1;
    }
    if (parser->position < parser->length &&
        (parser->source[parser->position] == 'e' ||
         parser->source[parser->position] == 'E'))
    {
        s16 exponentStart = parser->position++;
        if (parser->position < parser->length &&
            (parser->source[parser->position] == '+' ||
             parser->source[parser->position] == '-'))
            ++parser->position;
        {
            s16 digits = parser->position;
            while (parser->position < parser->length &&
                   isDigitChar(parser->source[parser->position]))
                ++parser->position;
            if (digits == parser->position)
            {
                parser->position = exponentStart;
                parser->error = CALC_ERR_SYNTAX;
                return -1;
            }
        }
    }
    (void)sawDecimal;
    index = makeNode(parser, CALC_SYNTAX_NUMBER, start, parser->position);
    if (index >= 0)
        copySpan(parser->ast->nodes[index].text, CALC_SYNTAX_TEXT,
                 parser->source, start, parser->position);
    return index;
}

static s16 scanIdentifier(SyntaxParser *parser, char *text, u8 capacity)
{
    s16 start = parser->position;
    s16 look;
    u8 count = 0;

    if ((startsText(parser->source + start, "sin^-1") ||
         startsText(parser->source + start, "cos^-1") ||
         startsText(parser->source + start, "tan^-1")) &&
        start + 6 < parser->length)
    {
        look = (s16)(start + 6);
        while (look < parser->length && isSpaceChar(parser->source[look]))
            ++look;
        if (look < parser->length && parser->source[look] == '(')
            parser->position = (s16)(start + 3);
    }
    else if (startsText(parser->source + start, "d2/dx2"))
    {
        look = (s16)(start + 6);
        while (look < parser->length && isSpaceChar(parser->source[look]))
            ++look;
        if (look < parser->length && parser->source[look] == '(')
            parser->position = (s16)(start + 6);
    }
    else if (startsText(parser->source + start, "d/dx"))
    {
        look = (s16)(start + 4);
        while (look < parser->length && isSpaceChar(parser->source[look]))
            ++look;
        if (look < parser->length && parser->source[look] == '(')
            parser->position = (s16)(start + 4);
    }

    if (parser->position == start)
    {
        look = start;
        while (look < parser->length && isIdentifierChar(parser->source[look]))
            ++look;

        /* A run followed by '(' is a function name.  Otherwise only named
           constants/memories are multi-character; adjacent letters are
           individual variables and are joined later by implicit multiply. */
        {
            s16 after = look;
            while (after < parser->length && isSpaceChar(parser->source[after]))
                ++after;
            if (after < parser->length && parser->source[after] == '(')
                parser->position = look;
            else if (look - start >= 6 &&
                     startsText(parser->source + start, "preans"))
                parser->position = (s16)(start + 6);
            else if (look - start >= 3 &&
                     startsText(parser->source + start, "ans"))
                parser->position = (s16)(start + 3);
            else if (look - start >= 3 &&
                     startsText(parser->source + start, "ran#"))
                parser->position = (s16)(start + 4);
            else if (look - start >= 2 &&
                     startsText(parser->source + start, "pi"))
                parser->position = (s16)(start + 2);
            else
                parser->position = (s16)(start + 1);
        }
    }
    while (start < parser->position && count + 1 < capacity)
        text[count++] = parser->source[start++];
    text[count] = '\0';
    return parser->position;
}

static s16 parsePrimary(SyntaxParser *parser)
{
    s16 start;
    s16 index;

    skipSpaces(parser);
    start = parser->position;
    if (start >= parser->length)
    {
        parser->error = CALC_ERR_SYNTAX;
        return -1;
    }
    if (isDigitChar(parser->source[start]) || parser->source[start] == '.' ||
        (parser->source[start] == ',' && !parser->commaSeparates))
        return parseNumber(parser);

    if (parser->source[start] == '(')
    {
        s16 child;
        ++parser->position;
        child = parseRelation(parser);
        skipSpaces(parser);
        if (child < 0 || parser->position >= parser->length ||
            parser->source[parser->position] != ')')
        {
            parser->error = CALC_ERR_SYNTAX;
            return -1;
        }
        ++parser->position;
        parser->ast->nodes[child].start = start;
        parser->ast->nodes[child].end = parser->position;
        return child;
    }

    if (isAlphaChar(parser->source[start]))
    {
        char name[CALC_SYNTAX_TEXT];
        s16 nameEnd = scanIdentifier(parser, name, sizeof(name));
        s16 look;

        skipSpaces(parser);
        look = parser->position;
        if ((equalText(name, "sin") || equalText(name, "cos") ||
             equalText(name, "tan")) &&
            look + 2 < parser->length && parser->source[look] == '^' &&
            parser->source[look + 1] == '-' &&
            parser->source[look + 2] == '1')
        {
            parser->position = (s16)(look + 3);
            skipSpaces(parser);
            if (parser->position < parser->length &&
                parser->source[parser->position] == '(')
            {
                u8 length = (u8)strlen(name);
                if ((u16)length + 3U < (u16)sizeof(name))
                {
                    name[length++] = '^';
                    name[length++] = '-';
                    name[length++] = '1';
                    name[length] = '\0';
                }
            }
            else
            {
                parser->position = nameEnd;
                skipSpaces(parser);
            }
        }

        if (parser->position < parser->length &&
            parser->source[parser->position] == '(')
        {
            CalcSyntaxNode *call;
            s16 open = parser->position;
            u8 savedComma = parser->commaSeparates;
            u8 usesSemicolon = topLevelSemicolon(parser, open);

            ++parser->position;
            index = makeNode(parser, CALC_SYNTAX_CALL, start, start);
            if (index < 0)
                return -1;
            call = &parser->ast->nodes[index];
            {
                u8 i = 0;
                while (name[i] && i + 1 < CALC_SYNTAX_TEXT)
                {
                    call->text[i] = name[i];
                    ++i;
                }
                call->text[i] = '\0';
            }
            parser->commaSeparates = (u8)(!usesSemicolon &&
                                                  !unaryFunctionName(name));
            skipSpaces(parser);
            if (parser->position < parser->length &&
                parser->source[parser->position] == ')')
            {
                ++parser->position;
            }
            else
            {
                for (;;)
                {
                    s16 argument;
                    if (call->argCount >= CALC_SYNTAX_MAX_ARGS)
                    {
                        parser->error = CALC_ERR_SYNTAX;
                        break;
                    }
                    argument = parseRelation(parser);
                    if (argument < 0)
                        break;
                    call->args[call->argCount++] = argument;
                    skipSpaces(parser);
                    if (parser->position >= parser->length)
                    {
                        parser->error = CALC_ERR_SYNTAX;
                        break;
                    }
                    if (parser->source[parser->position] == ')')
                    {
                        ++parser->position;
                        break;
                    }
                    if (parser->source[parser->position] == ';' ||
                        (parser->source[parser->position] == ',' &&
                         parser->commaSeparates))
                    {
                        ++parser->position;
                        skipSpaces(parser);
                        continue;
                    }
                    parser->error = CALC_ERR_SYNTAX;
                    break;
                }
            }
            parser->commaSeparates = savedComma;
            if (parser->error)
                return -1;
            call->end = parser->position;
            return index;
        }

        index = makeNode(parser, CALC_SYNTAX_IDENTIFIER, start,
                         parser->position);
        if (index >= 0)
        {
            CalcSyntaxNode *identifier = &parser->ast->nodes[index];
            u8 i = 0;
            while (name[i] && i + 1 < CALC_SYNTAX_TEXT)
            {
                identifier->text[i] = name[i];
                ++i;
            }
            identifier->text[i] = '\0';
        }
        return index;
    }

    parser->error = CALC_ERR_SYNTAX;
    return -1;
}

static s16 parsePostfix(SyntaxParser *parser)
{
    s16 child = parsePrimary(parser);

    for (;;)
    {
        s16 index;
        CalcSyntaxNode *node;
        char op;

        skipSpaces(parser);
        if (parser->position >= parser->length ||
            (parser->source[parser->position] != '!' &&
             parser->source[parser->position] != '%') ||
            (parser->source[parser->position] == '!' &&
             parser->position + 1 < parser->length &&
             parser->source[parser->position + 1] == '='))
            break;
        op = parser->source[parser->position++];
        index = makeNode(parser, CALC_SYNTAX_POSTFIX,
                         parser->ast->nodes[child].start, parser->position);
        if (index < 0)
            return -1;
        node = &parser->ast->nodes[index];
        node->op = op;
        node->left = child;
        child = index;
    }
    return child;
}

static s16 parseUnary(SyntaxParser *parser);

static s16 parsePower(SyntaxParser *parser)
{
    s16 left = parsePostfix(parser);

    skipSpaces(parser);
    if (!parser->error && parser->position < parser->length &&
        parser->source[parser->position] == '^')
    {
        s16 right;
        ++parser->position;
        right = parseUnary(parser);
        left = makeBinary(parser, CALC_SYNTAX_BINARY, '^', "^", left,
                          right);
    }
    return left;
}

static s16 parseUnary(SyntaxParser *parser)
{
    s16 start;

    skipSpaces(parser);
    start = parser->position;
    if (start < parser->length &&
        (parser->source[start] == '+' || parser->source[start] == '-'))
    {
        CalcSyntaxNode *node;
        char op = parser->source[parser->position++];
        s16 child = parseUnary(parser);
        s16 index;

        if (child < 0)
            return -1;
        index = makeNode(parser, CALC_SYNTAX_UNARY, start,
                         parser->ast->nodes[child].end);
        if (index < 0)
            return -1;
        node = &parser->ast->nodes[index];
        node->op = op;
        node->left = child;
        return index;
    }
    return parsePower(parser);
}

static u8 startsImplicitPrimary(SyntaxParser *parser)
{
    char value;

    skipSpaces(parser);
    if (parser->position >= parser->length)
        return 0;
    value = parser->source[parser->position];
    if (value == '(' || isAlphaChar(value) || isDigitChar(value) ||
        value == '.')
        return 1;
    return (u8)(value == ',' && !parser->commaSeparates);
}

static s16 parseProduct(SyntaxParser *parser)
{
    s16 left = parseUnary(parser);

    while (!parser->error)
    {
        char op = 0;
        s16 right;

        skipSpaces(parser);
        if (parser->position < parser->length &&
            (parser->source[parser->position] == '*' ||
             parser->source[parser->position] == '/'))
            op = parser->source[parser->position++];
        else if (startsImplicitPrimary(parser))
            op = '*';
        else
            break;
        right = parseUnary(parser);
        left = makeBinary(parser, CALC_SYNTAX_BINARY, op,
                          op == '*' ? "*" : "/", left, right);
    }
    return left;
}

static s16 parseSum(SyntaxParser *parser)
{
    s16 left = parseProduct(parser);

    while (!parser->error)
    {
        char op;
        s16 right;

        skipSpaces(parser);
        if (parser->position >= parser->length ||
            (parser->source[parser->position] != '+' &&
             parser->source[parser->position] != '-'))
            break;
        op = parser->source[parser->position++];
        right = parseProduct(parser);
        left = makeBinary(parser, CALC_SYNTAX_BINARY, op,
                          op == '+' ? "+" : "-", left, right);
    }
    return left;
}

static s16 parseRelation(SyntaxParser *parser)
{
    s16 left = parseSum(parser);

    skipSpaces(parser);
    if (!parser->error && parser->position < parser->length)
    {
        const char *relation = NULL;
        char op = 0;
        s16 width = 0;
        char first = parser->source[parser->position];
        char second = parser->position + 1 < parser->length
                          ? parser->source[parser->position + 1]
                          : '\0';

        if (first == '<' && second == '=')
        {
            relation = "<=";
            op = 'L';
            width = 2;
        }
        else if (first == '>' && second == '=')
        {
            relation = ">=";
            op = 'G';
            width = 2;
        }
        else if (first == '!' && second == '=')
        {
            relation = "!=";
            op = '!';
            width = 2;
        }
        else if (first == '=' && second == '=')
        {
            relation = "=";
            op = '=';
            width = 2;
        }
        else if (first == '=' || first == '<' || first == '>')
        {
            relation = first == '=' ? "=" : (first == '<' ? "<" : ">");
            op = first;
            width = 1;
        }
        if (width)
        {
            s16 right;
            parser->position = (s16)(parser->position + width);
            right = parseSum(parser);
            left = makeBinary(parser, CALC_SYNTAX_RELATION, op, relation,
                              left, right);
        }
    }
    return left;
}

u8 calcSyntaxParse(const char *source, CalcSyntaxAst *ast)
{
    SyntaxParser parser;

    if (source == NULL || ast == NULL)
        return CALC_ERR_SYNTAX;
    memset(ast, 0, sizeof(*ast));
    ast->root = -1;
    parser.source = source;
    parser.length = 0;
    while (source[parser.length] != '\0' && parser.length < 32767)
        ++parser.length;
    parser.position = 0;
    parser.ast = ast;
    parser.error = CALC_OK;
    parser.commaSeparates = 0;
    skipSpaces(&parser);
    if (parser.position == parser.length)
        parser.error = CALC_ERR_SYNTAX;
    else
        ast->root = parseRelation(&parser);
    skipSpaces(&parser);
    if (!parser.error && parser.position != parser.length)
        parser.error = CALC_ERR_SYNTAX;
    if (parser.error || ast->root < 0)
    {
        ast->root = -1;
        ast->error = parser.error ? parser.error : CALC_ERR_SYNTAX;
        return ast->error;
    }
    ast->error = CALC_OK;
    return CALC_OK;
}

u8 calcSyntaxAstHasCall(const CalcSyntaxAst *ast, const char *name)
{
    u8 i;

    if (ast == NULL || name == NULL)
        return 0;
    for (i = 0; i < ast->count; ++i)
    {
        if (ast->nodes[i].kind == CALC_SYNTAX_CALL &&
            equalText(ast->nodes[i].text, name))
            return 1;
    }
    return 0;
}

u8 calcSyntaxAstHasOperator(const CalcSyntaxAst *ast, char op)
{
    u8 i;

    if (ast == NULL)
        return 0;
    for (i = 0; i < ast->count; ++i)
    {
        const CalcSyntaxNode *node = &ast->nodes[i];
        if ((node->kind == CALC_SYNTAX_UNARY ||
             node->kind == CALC_SYNTAX_BINARY ||
             node->kind == CALC_SYNTAX_RELATION ||
             node->kind == CALC_SYNTAX_POSTFIX) && node->op == op)
            return 1;
    }
    return 0;
}

u8 calcSyntaxAstNodeSpan(const CalcSyntaxAst *ast, s16 node,
                         s16 *start, s16 *end)
{
    if (ast == NULL || start == NULL || end == NULL || node < 0 ||
        node >= ast->count)
        return 0;
    *start = ast->nodes[node].start;
    *end = ast->nodes[node].end;
    return 1;
}

static void trimBounds(const char *source, s16 *start, s16 *end)
{
    while (*start < *end && isSpaceChar(source[*start]))
        ++*start;
    while (*end > *start && isSpaceChar(source[*end - 1]))
        --*end;
}

static u8 copyBoundedFormula(char *destination, const char *source,
                             s16 start, s16 end)
{
    trimBounds(source, &start, &end);
    if (end <= start || end - start >= CALC_SYNTAX_TEXT)
        return 0;
    copySpan(destination, CALC_SYNTAX_TEXT, source, start, end);
    return 1;
}

static s16 sourceLength(const char *source)
{
    s16 length = 0;
    if (source == NULL)
        return 0;
    while (source[length] && length < 32767)
        ++length;
    return length;
}

static s16 findTopLevelSeparator(const char *source, s16 start, s16 end,
                                  char separator)
{
    s16 depth = 0;
    s16 position;

    for (position = start; position < end; ++position)
    {
        if (source[position] == '(')
            ++depth;
        else if (source[position] == ')')
        {
            if (depth == 0)
                return -1;
            --depth;
        }
        else if (source[position] == separator && depth == 0)
            return position;
    }
    return -1;
}

u8 calcSyntaxParseGraphRow(const char *source, CalcSyntaxGraphRow *row)
{
    s16 start = 0;
    s16 end;
    s16 separator;
    s16 relation = -1;
    s16 relationWidth = 0;
    CalcSyntaxAst check;

    if (source == NULL || row == NULL)
        return CALC_ERR_SYNTAX;
    memset(row, 0, sizeof(*row));
    end = sourceLength(source);
    trimBounds(source, &start, &end);
    if (end <= start)
        return CALC_ERR_SYNTAX;

    if ((end - start >= 6 && startsText(source + start, "param:")) ||
        (end - start >= 8 && startsText(source + start, "param(") &&
         source[end - 1] == ')'))
    {
        u8 canonical = (u8)(source[start + 5] == '(');
        start = (s16)(start + 6);
        if (canonical)
            end--;
        separator = findTopLevelSeparator(source, start, end, ';');
        if (separator < 0 ||
            !copyBoundedFormula(row->formulaA, source, start, separator) ||
            !copyBoundedFormula(row->formulaB, source, separator + 1, end) ||
            calcSyntaxParse(row->formulaA, &check) != CALC_OK ||
            calcSyntaxParse(row->formulaB, &check) != CALC_OK)
            return CALC_ERR_SYNTAX;
        row->kind = CALC_GRAPH_ROW_PARAM;
        return CALC_OK;
    }

    {
        s16 position;
        s16 depth = 0;
        for (position = start; position < end; ++position)
        {
            char value = source[position];
            if (value == '(')
                ++depth;
            else if (value == ')')
            {
                if (depth == 0)
                    return CALC_ERR_SYNTAX;
                --depth;
            }
            else if (depth == 0 && (value == '=' || value == '<' ||
                                     value == '>' || value == '!'))
            {
                relation = position;
                if (position + 1 < end && source[position + 1] == '=')
                    relationWidth = 2;
                else if (value != '!')
                    relationWidth = 1;
                break;
            }
        }
    }
    if (relation < 0 || relationWidth == 0)
        return CALC_ERR_SYNTAX;

    {
        s16 leftStart = start;
        s16 leftEnd = relation;
        trimBounds(source, &leftStart, &leftEnd);
        if (leftEnd - leftStart != 1)
            return CALC_ERR_SYNTAX;
        row->axisX = (u8)(lowerAscii(source[leftStart]) == 'x');
        if (!row->axisX && lowerAscii(source[leftStart]) != 'y' &&
            lowerAscii(source[leftStart]) != 'r')
            return CALC_ERR_SYNTAX;
        row->relation[0] = source[relation];
        if (relationWidth == 2)
            row->relation[1] = '=';
        row->relation[relationWidth] = '\0';
        if (!copyBoundedFormula(row->formulaA, source,
                                (s16)(relation + relationWidth), end) ||
            calcSyntaxParse(row->formulaA, &check) != CALC_OK)
            return CALC_ERR_SYNTAX;
        if (source[relation] == '<' || source[relation] == '>' ||
            source[relation] == '!')
            row->kind = CALC_GRAPH_ROW_INEQUALITY;
        else if (lowerAscii(source[leftStart]) == 'x')
            row->kind = CALC_GRAPH_ROW_X;
        else if (lowerAscii(source[leftStart]) == 'r')
            row->kind = CALC_GRAPH_ROW_POLAR;
        else
            row->kind = CALC_GRAPH_ROW_Y;
    }
    return CALC_OK;
}

u8 calcSyntaxFindCall(const char *source, const char *name,
                      char *inside, u8 insideSize, u8 *wholeCall)
{
    s16 length;
    s16 nameLength;
    s16 position;

    if (source == NULL || name == NULL || inside == NULL || insideSize == 0)
        return 0;
    length = sourceLength(source);
    nameLength = sourceLength(name);
    for (position = 0; position + nameLength <= length; ++position)
    {
        s16 look;
        s16 open;
        s16 close;
        s16 depth;
        s16 before;
        s16 after;

        if (position > 0 && isIdentifierChar(source[position - 1]))
            continue;
        if (!startsText(source + position, name))
            continue;
        look = (s16)(position + nameLength);
        if (look < length && isIdentifierChar(source[look]))
            continue;
        while (look < length && isSpaceChar(source[look]))
            ++look;
        if (look >= length || source[look] != '(')
            continue;
        open = look;
        depth = 1;
        close = (s16)(open + 1);
        while (close < length && depth > 0)
        {
            if (source[close] == '(')
                ++depth;
            else if (source[close] == ')')
                --depth;
            ++close;
        }
        if (depth != 0)
            return 0;
        if (close - open - 2 >= insideSize)
            return 0;
        copySpan(inside, insideSize, source, (s16)(open + 1),
                 (s16)(close - 1));
        if (wholeCall != NULL)
        {
            before = 0;
            while (before < position && isSpaceChar(source[before]))
                ++before;
            after = close;
            while (after < length && isSpaceChar(source[after]))
                ++after;
            *wholeCall = (u8)(before == position && after == length);
        }
        return 1;
    }
    return 0;
}

u8 calcSyntaxExtractCall(const char *source, const char *name,
                         char *inside, u8 insideSize)
{
    return calcSyntaxFindCall(source, name, inside, insideSize, NULL);
}

u8 calcSyntaxSplitArgs(const char *source, char *args, u8 stride,
                       u8 maximum, u8 *count)
{
    s16 length;
    s16 start;
    s16 position;
    s16 depth = 0;
    char separator;
    u8 output = 0;

    if (source == NULL || args == NULL || stride == 0 || maximum == 0 ||
        count == NULL)
        return 0;
    length = sourceLength(source);
    separator = findTopLevelSeparator(source, 0, length, ';') >= 0 ? ';' : ',';
    start = 0;
    for (position = 0; position <= length; ++position)
    {
        char value = position < length ? source[position] : '\0';
        if (value == '(')
            ++depth;
        else if (value == ')')
        {
            if (depth == 0)
                return 0;
            --depth;
        }
        if ((position == length || (value == separator && depth == 0)))
        {
            s16 itemStart = start;
            s16 itemEnd = position;
            char *destination;
            trimBounds(source, &itemStart, &itemEnd);
            if (itemEnd <= itemStart || itemEnd - itemStart >= stride ||
                output >= maximum)
                return 0;
            destination = args + (u16)output * stride;
            copySpan(destination, stride, source, itemStart, itemEnd);
            ++output;
            start = (s16)(position + 1);
        }
    }
    if (depth != 0 || output == 0)
        return 0;
    *count = output;
    return 1;
}
