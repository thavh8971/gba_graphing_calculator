#ifndef GCALC_SYNTAX_H
#define GCALC_SYNTAX_H

#include "gcalc/calc.h"

#define CALC_SYNTAX_MAX_NODES 96
#define CALC_SYNTAX_TEXT 64
#define CALC_SYNTAX_MAX_ARGS 6

typedef enum CalcSyntaxNodeKind {
    CALC_SYNTAX_NUMBER,
    CALC_SYNTAX_IDENTIFIER,
    CALC_SYNTAX_UNARY,
    CALC_SYNTAX_BINARY,
    CALC_SYNTAX_CALL,
    CALC_SYNTAX_RELATION,
    CALC_SYNTAX_POSTFIX
} CalcSyntaxNodeKind;

typedef struct CalcSyntaxNode {
    CalcSyntaxNodeKind kind;
    char op;
    char text[CALC_SYNTAX_TEXT];
    s16 start;
    s16 end;
    s16 left;
    s16 right;
    s16 args[CALC_SYNTAX_MAX_ARGS];
    u8 argCount;
} CalcSyntaxNode;

typedef struct CalcSyntaxAst {
    CalcSyntaxNode nodes[CALC_SYNTAX_MAX_NODES];
    s16 root;
    u8 count;
    u8 error;
} CalcSyntaxAst;

typedef enum CalcSyntaxGraphKind {
    CALC_GRAPH_ROW_Y,
    CALC_GRAPH_ROW_X,
    CALC_GRAPH_ROW_POLAR,
    CALC_GRAPH_ROW_PARAM,
    CALC_GRAPH_ROW_INEQUALITY
} CalcSyntaxGraphKind;

typedef struct CalcSyntaxGraphRow {
    CalcSyntaxGraphKind kind;
    u8 axisX;
    char relation[3];
    char formulaA[CALC_SYNTAX_TEXT];
    char formulaB[CALC_SYNTAX_TEXT];
} CalcSyntaxGraphRow;

u8 calcSyntaxParse(const char *source, CalcSyntaxAst *ast);
u8 calcSyntaxAstHasCall(const CalcSyntaxAst *ast, const char *name);
u8 calcSyntaxAstHasOperator(const CalcSyntaxAst *ast, char op);
u8 calcSyntaxAstNodeSpan(const CalcSyntaxAst *ast, s16 node,
                         s16 *start, s16 *end);
u8 calcSyntaxParseGraphRow(const char *source, CalcSyntaxGraphRow *row);
u8 calcSyntaxFindCall(const char *source, const char *name,
                      char *inside, u8 insideSize, u8 *wholeCall);
u8 calcSyntaxExtractCall(const char *source, const char *name,
                         char *inside, u8 insideSize);
u8 calcSyntaxSplitArgs(const char *source, char *args, u8 stride,
                       u8 maximum, u8 *count);

#endif
