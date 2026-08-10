#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "mode_internal.h"

static s32 digitValue(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

u8 modeBaseDigitValid(BaseRadix radix, char digit)
{
    s32 value = digitValue(digit);
    return value >= 0 && value < (s32)radix &&
           (radix == BASE_RADIX_BIN || radix == BASE_RADIX_OCT ||
            radix == BASE_RADIX_DEC || radix == BASE_RADIX_HEX);
}

static u8 formatUnsignedBase(u32 value, BaseRadix radix, char *output,
                             u16 capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    char reverse[33];
    u8 count = 0;
    u8 index;

    if (output == 0 || capacity == 0)
        return 0;
    do {
        reverse[count++] = digits[value % (u32)radix];
        value /= (u32)radix;
    } while (value != 0 && count < sizeof(reverse));
    if ((u16)count + 1 > capacity) {
        output[0] = '\0';
        return 0;
    }
    for (index = 0; index < count; index++)
        output[index] = reverse[count - index - 1];
    output[count] = '\0';
    return 1;
}

u8 modeBaseFormatValue(BaseRadix radix, s32 value, char *output,
                       u16 capacity)
{
    int written;

    if (output == 0 || capacity == 0)
        return 0;
    if (radix == BASE_RADIX_DEC) {
        written = snprintf(output, capacity, "%ld", (long)value);
        return written >= 0 && written < (int)capacity;
    }
    if (radix != BASE_RADIX_BIN && radix != BASE_RADIX_OCT &&
        radix != BASE_RADIX_HEX) {
        output[0] = '\0';
        return 0;
    }
    return formatUnsignedBase((u32)value, radix, output, capacity);
}

static void setBaseResult(ModeRuntime *runtime, s32 value)
{
    char binary[34];
    char octal[13];
    char decimal[13];
    char hexadecimal[10];
    char output[MODE_RESULT_CAPACITY];

    modeBaseFormatValue(BASE_RADIX_BIN, value, binary, sizeof(binary));
    modeBaseFormatValue(BASE_RADIX_OCT, value, octal, sizeof(octal));
    modeBaseFormatValue(BASE_RADIX_DEC, value, decimal, sizeof(decimal));
    modeBaseFormatValue(BASE_RADIX_HEX, value, hexadecimal,
                        sizeof(hexadecimal));
    switch (modeGetBaseRadix(runtime)) {
    case BASE_RADIX_BIN:
        snprintf(output, sizeof(output), "BIN=%s DEC=%s O=%s H=%s",
                 binary, decimal, octal, hexadecimal);
        break;
    case BASE_RADIX_OCT:
        snprintf(output, sizeof(output), "OCT=%s DEC=%s B=%s H=%s",
                 octal, decimal, binary, hexadecimal);
        break;
    case BASE_RADIX_HEX:
        snprintf(output, sizeof(output), "HEX=%s DEC=%s B=%s O=%s",
                 hexadecimal, decimal, binary, octal);
        break;
    default:
        snprintf(output, sizeof(output), "DEC=%s B=%s O=%s H=%s",
                 decimal, binary, octal, hexadecimal);
        break;
    }
    modeSetResult(runtime, output, CALC_OK);
}

static u8 radixForName(const char *name, BaseRadix *radix)
{
    if (modeEquals(name, "bin"))
        *radix = BASE_RADIX_BIN;
    else if (modeEquals(name, "oct"))
        *radix = BASE_RADIX_OCT;
    else if (modeEquals(name, "dec"))
        *radix = BASE_RADIX_DEC;
    else if (modeEquals(name, "hex"))
        *radix = BASE_RADIX_HEX;
    else
        return 0;
    return 1;
}

static u8 legacyRadixPrefix(const char *expression, BaseRadix *radix,
                            const char **body)
{
    const char *colon = strchr(expression, ':');
    u16 length;

    if (colon == 0)
        return 0;
    length = (u16)(colon - expression);
    if (length == 1 && expression[0] == '2')
        *radix = BASE_RADIX_BIN;
    else if (length == 1 && expression[0] == '8')
        *radix = BASE_RADIX_OCT;
    else if (length == 2 && expression[0] == '1' && expression[1] == '0')
        *radix = BASE_RADIX_DEC;
    else if (length == 2 && expression[0] == '1' && expression[1] == '6')
        *radix = BASE_RADIX_HEX;
    else
        return 0;
    *body = colon + 1;
    return 1;
}

typedef struct BaseParser {
    ModeRuntime *runtime;
    const char *text;
    u16 position;
    BaseRadix radix;
    u8 depth;
    u8 error;
} BaseParser;

static s32 baseExpression(BaseParser *parser);
static u8 evaluateBaseText(ModeRuntime *runtime, const char *text,
                           BaseRadix radix, u8 depth, s32 *value);

static void baseSkip(BaseParser *parser)
{
    while (parser->text[parser->position] == ' ' ||
           parser->text[parser->position] == '\t')
        parser->position++;
}

static u8 checkedArithmetic(char operation, s32 left, s32 right,
                            s32 *result)
{
    s64 value;

    if (operation == '/') {
        if (right == 0)
            return CALC_ERR_DIVZERO;
        if (left == INT32_MIN && right == -1)
            return CALC_ERR_RANGE;
        *result = left / right;
        return CALC_OK;
    }
    if (operation == '+')
        value = (s64)left + (s64)right;
    else if (operation == '-')
        value = (s64)left - (s64)right;
    else
        value = (s64)left * (s64)right;
    if (value < INT32_MIN || value > INT32_MAX)
        return CALC_ERR_RANGE;
    *result = (s32)value;
    return CALC_OK;
}

static u8 evaluateBaseFunction(ModeRuntime *runtime, const char *name,
                               const char *body, BaseRadix radix, u8 depth,
                               s32 *value)
{
    char fields[2][64];
    BaseRadix explicitRadix;
    s32 left;
    s32 right = 0;
    u8 count;
    u8 error;

    if (depth >= 12)
        return CALC_ERR_RANGE;
    if (radixForName(name, &explicitRadix))
        return evaluateBaseText(runtime, body, explicitRadix,
                                (u8)(depth + 1), value);
    if (!modeSplit(body, ';', fields, 2, &count))
        return CALC_ERR_SYNTAX;
    if (modeEquals(name, "not") || modeEquals(name, "neg")) {
        if (count != 1)
            return CALC_ERR_SYNTAX;
        error = evaluateBaseText(runtime, fields[0], radix,
                                 (u8)(depth + 1), &left);
        if (error != CALC_OK)
            return error;
        *value = modeEquals(name, "not") ? (s32)~(u32)left :
                                           (s32)(0U - (u32)left);
        return CALC_OK;
    }
    if (count != 2)
        return CALC_ERR_SYNTAX;
    error = evaluateBaseText(runtime, fields[0], radix,
                             (u8)(depth + 1), &left);
    if (error == CALC_OK)
        error = evaluateBaseText(runtime, fields[1], radix,
                                 (u8)(depth + 1), &right);
    if (error != CALC_OK)
        return error;
    if (modeEquals(name, "and"))
        *value = (s32)((u32)left & (u32)right);
    else if (modeEquals(name, "or"))
        *value = (s32)((u32)left | (u32)right);
    else if (modeEquals(name, "xor"))
        *value = (s32)((u32)left ^ (u32)right);
    else if (modeEquals(name, "xnor"))
        *value = (s32)~((u32)left ^ (u32)right);
    else if (modeEquals(name, "shl")) {
        if (right < 0 || right > 31)
            return CALC_ERR_RANGE;
        *value = (s32)((u32)left << (u8)right);
    } else if (modeEquals(name, "shr")) {
        if (right < 0 || right > 31)
            return CALC_ERR_RANGE;
        *value = (s32)((u32)left >> (u8)right);
    } else
        return CALC_ERR_SYNTAX;
    return CALC_OK;
}

static s32 baseNumber(BaseParser *parser, u8 negativeDecimal)
{
    u32 value = 0;
    u32 limit = parser->radix == BASE_RADIX_DEC ?
                (negativeDecimal ? 0x80000000UL : 0x7fffffffUL) :
                0xffffffffUL;
    u8 any = 0;

    while (1) {
        s32 digit = digitValue(parser->text[parser->position]);
        if (digit < 0 || digit >= (s32)parser->radix)
            break;
        if (value > (limit - (u32)digit) / (u32)parser->radix) {
            /* A literal outside the selected 32-bit entry range is invalid
               input.  Arithmetic overflow is reported separately as Range
               ERROR by checkedArithmetic(). */
            parser->error = CALC_ERR_SYNTAX;
            return 0;
        }
        value = value * (u32)parser->radix + (u32)digit;
        parser->position++;
        any = 1;
    }
    if (!any) {
        parser->error = CALC_ERR_SYNTAX;
        return 0;
    }
    if (negativeDecimal) {
        if (value == 0x80000000UL)
            return INT32_MIN;
        return -(s32)value;
    }
    return (s32)value;
}

static s32 basePrimary(BaseParser *parser)
{
    u16 start;

    baseSkip(parser);
    if (parser->text[parser->position] == '(') {
        s32 value;
        parser->position++;
        value = baseExpression(parser);
        baseSkip(parser);
        if (parser->text[parser->position] != ')')
            parser->error = CALC_ERR_SYNTAX;
        else
            parser->position++;
        return value;
    }

    start = parser->position;
    if ((parser->text[start] >= 'A' && parser->text[start] <= 'Z') ||
        (parser->text[start] >= 'a' && parser->text[start] <= 'z') ||
        parser->text[start] == '_') {
        u16 cursor = start + 1;
        u16 afterName;
        while ((parser->text[cursor] >= 'A' &&
                parser->text[cursor] <= 'Z') ||
               (parser->text[cursor] >= 'a' &&
                parser->text[cursor] <= 'z') ||
               (parser->text[cursor] >= '0' &&
                parser->text[cursor] <= '9') ||
               parser->text[cursor] == '_')
            cursor++;
        afterName = cursor;
        while (parser->text[cursor] == ' ' || parser->text[cursor] == '\t')
            cursor++;
        if (parser->text[cursor] == '(') {
            char name[16];
            char body[160];
            u16 nameLength = (u16)(afterName - start);
            u16 bodyStart = (u16)(cursor + 1);
            s16 nested = 1;
            s32 value = 0;
            u8 error;

            cursor++;
            while (parser->text[cursor] != '\0' && nested != 0) {
                if (parser->text[cursor] == '(')
                    nested++;
                else if (parser->text[cursor] == ')')
                    nested--;
                cursor++;
            }
            if (nested != 0 || (size_t)nameLength + 1U > sizeof(name) ||
                (size_t)(cursor - bodyStart) > sizeof(body)) {
                parser->error = CALC_ERR_SYNTAX;
                return 0;
            }
            memcpy(name, parser->text + start, nameLength);
            name[nameLength] = '\0';
            memcpy(body, parser->text + bodyStart,
                   (size_t)(cursor - bodyStart - 1));
            body[cursor - bodyStart - 1] = '\0';
            parser->position = cursor;
            error = evaluateBaseFunction(parser->runtime, name, body,
                                         parser->radix, parser->depth,
                                         &value);
            if (error != CALC_OK)
                parser->error = error;
            return value;
        }
    }
    return baseNumber(parser, 0);
}

static s32 baseUnary(BaseParser *parser)
{
    char operation;
    s32 value;

    baseSkip(parser);
    operation = parser->text[parser->position];
    if (operation != '+' && operation != '-')
        return basePrimary(parser);
    parser->position++;
    if (operation == '+')
        return baseUnary(parser);
    if (parser->radix != BASE_RADIX_DEC) {
        parser->error = CALC_ERR_SYNTAX;
        return 0;
    }
    baseSkip(parser);
    if (digitValue(parser->text[parser->position]) >= 0)
        return baseNumber(parser, 1);
    value = baseUnary(parser);
    if (parser->error != CALC_OK)
        return 0;
    if (value == INT32_MIN) {
        parser->error = CALC_ERR_RANGE;
        return 0;
    }
    return -value;
}

static s32 baseTerm(BaseParser *parser)
{
    s32 value = baseUnary(parser);

    while (parser->error == CALC_OK) {
        char operation;
        s32 right;
        s32 result;
        u8 error;
        baseSkip(parser);
        operation = parser->text[parser->position];
        if (operation != '*' && operation != '/')
            break;
        parser->position++;
        right = baseUnary(parser);
        if (parser->error != CALC_OK)
            break;
        error = checkedArithmetic(operation, value, right, &result);
        if (error != CALC_OK) {
            parser->error = error;
            break;
        }
        value = result;
    }
    return value;
}

static s32 baseExpression(BaseParser *parser)
{
    s32 value = baseTerm(parser);

    while (parser->error == CALC_OK) {
        char operation;
        s32 right;
        s32 result;
        u8 error;
        baseSkip(parser);
        operation = parser->text[parser->position];
        if (operation != '+' && operation != '-')
            break;
        parser->position++;
        right = baseTerm(parser);
        if (parser->error != CALC_OK)
            break;
        error = checkedArithmetic(operation, value, right, &result);
        if (error != CALC_OK) {
            parser->error = error;
            break;
        }
        value = result;
    }
    return value;
}

static u8 evaluateBaseText(ModeRuntime *runtime, const char *text,
                           BaseRadix radix, u8 depth, s32 *value)
{
    BaseParser parser;

    parser.runtime = runtime;
    parser.text = text;
    parser.position = 0;
    parser.radix = radix;
    parser.depth = depth;
    parser.error = CALC_OK;
    *value = baseExpression(&parser);
    baseSkip(&parser);
    if (parser.error == CALC_OK && parser.text[parser.position] != '\0')
        parser.error = CALC_ERR_SYNTAX;
    return parser.error;
}

u8 modeBaseEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[16];
    char arguments[256];
    BaseRadix explicitRadix;
    const char *legacyBody;
    s32 value;
    u8 error;

    if (legacyRadixPrefix(expression, &explicitRadix, &legacyBody)) {
        error = evaluateBaseText(runtime, legacyBody, explicitRadix, 0,
                                 &value);
        if (error != CALC_OK)
            return error;
        modeSetBaseRadix(runtime, explicitRadix);
        setBaseResult(runtime, value);
        return CALC_OK;
    }
    if (!modeParseCommand(expression, name, sizeof(name), arguments,
                          sizeof(arguments))) {
        error = evaluateBaseText(runtime, expression,
                                 modeGetBaseRadix(runtime), 0, &value);
        if (error != CALC_OK)
            return error;
        setBaseResult(runtime, value);
        return CALC_OK;
    }
    if (radixForName(name, &explicitRadix)) {
        error = evaluateBaseText(runtime, arguments, explicitRadix, 0,
                                 &value);
        if (error != CALC_OK)
            return error;
        modeSetBaseRadix(runtime, explicitRadix);
        setBaseResult(runtime, value);
        return CALC_OK;
    }
    /* Historical name:arguments saves were decimal regardless of the last
       selected display radix.  Canonical name(arguments) follows the active
       radix, matching direct BASE-N arithmetic. */
    error = evaluateBaseFunction(runtime, name, arguments,
                                 strchr(expression, ':') != 0 ?
                                 BASE_RADIX_DEC : modeGetBaseRadix(runtime),
                                 0, &value);
    if (error != CALC_OK)
        return error;
    setBaseResult(runtime, value);
    return CALC_OK;
}
