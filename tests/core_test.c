#include "gcalc/calc.h"
#include "gcalc/syntax.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expectNear(CalcContext *context, const char *expression,
                       long double expected, long double tolerance)
{
    CalcNumber result;
    long double actual;
    u8 error;

    calcEvaluateContext(context, expression, &result, &error);
    actual = calcNumberToLongDouble(result);
    if (error != CALC_OK || fabsl(actual - expected) > tolerance)
    {
        printf("FAIL: %s -> error %u, value %.12g (expected %.12g)\n",
               expression, error, (double)actual, (double)expected);
        ++failures;
    }
}

static void expectSyntax(const char *expression)
{
    CalcSyntaxAst ast;
    if (calcSyntaxParse(expression, &ast) != CALC_OK || ast.root < 0)
    {
        printf("FAIL syntax: %s\n", expression);
        ++failures;
    }
}

static void testParseCache(CalcContext *context)
{
    char mutableExpression[32];
    u8 repeat;

    strcpy(mutableExpression, "2+3");
    for (repeat = 0; repeat < 8; repeat++)
        expectNear(context, mutableExpression, 5.0L, 0.0L);

    /* Cache identity is expression content, not the caller's pointer.  This
       catches a stale AST when an editor mutates its reusable input buffer. */
    strcpy(mutableExpression, "8*7");
    expectNear(context, mutableExpression, 56.0L, 0.0L);
    strcpy(mutableExpression, "2+3");
    expectNear(context, mutableExpression, 5.0L, 0.0L);
}

int main(void)
{
    CalcContext radians;
    CalcContext degrees;
    CalcNumber two;
    CalcNumber result;
    CalcSyntaxAst ast;
    u8 error;

    calcContextInit(&radians);
    calcContextInit(&degrees);
    degrees.angleMode = CALC_ANGLE_DEG;
    two = calcNumberFromLongDouble(2.0L, &error);
    calcContextSetVariable(&radians, 'A', two);
    expectNear(&radians, "2A+3(4+1)", 19.0L, 1.0e-9L);
    expectNear(&radians, "1,25+2.75+1E-2", 4.01L, 1.0e-9L);
    expectNear(&radians, "-2^2+5!+50%", 116.5L, 1.0e-8L);
    expectNear(&radians, "3<=3", 1.0L, 0.0L);

    expectNear(&radians, "sum(x^2;x;1;3)", 14.0L, 1.0e-8L);
    expectNear(&radians, "sum(1/x;1;1000)", 7.485470861L, 2.0e-8L);
    expectNear(&radians, "prod(x;x;1;4)", 24.0L, 1.0e-8L);
    expectNear(&radians, "integral(x^2;x;0;1)", 1.0L / 3.0L, 2.0e-8L);
    expectNear(&radians, "d/dx(x^2;x;3)", 6.0L, 2.0e-6L);
    expectNear(&radians, "d2/dx2(x^2;x;3)", 2.0L, 2.0e-5L);

    expectNear(&radians, "sin(pi/2)", 1.0L, 2.0e-8L);
    expectNear(&degrees, "sin(90)", 1.0L, 2.0e-8L);
    expectNear(&radians, "A", 2.0L, 0.0L);
    expectNear(&degrees, "A", 0.0L, 0.0L);

    calcEvaluateContext(&radians, "7", &result, &error);
    calcEvaluateContext(&degrees, "11", &result, &error);
    expectNear(&radians, "Ans", 7.0L, 0.0L);
    expectNear(&degrees, "Ans", 11.0L, 0.0L);

    testParseCache(&radians);

    expectSyntax("2xy+sin^-1(1)");
    expectSyntax("normalpdf(0,0,1)");
    expectSyntax("atanh(0,5)");
    if (calcSyntaxParse("2x+sin(x)^2", &ast) != CALC_OK ||
        !calcSyntaxAstHasOperator(&ast, '*') ||
        !calcSyntaxAstHasCall(&ast, "sin"))
    {
        puts("FAIL AST query helpers");
        ++failures;
    }

    if (failures)
        return 1;
    puts("PASS: core parser/evaluator");
    return 0;
}
