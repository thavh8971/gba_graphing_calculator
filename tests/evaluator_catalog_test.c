#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcalc/calc.h"
#include "gcalc/syntax.h"

static CalcContext context;

static void fail(const char *expression, const char *message)
{
    fprintf(stderr, "FAIL %s: %s\n", expression, message);
    exit(1);
}

static CalcNumber evaluate(const char *expression, u8 expectedError)
{
    CalcNumber value;
    u8 error;

    calcEvaluateContext(&context, expression, &value, &error);
    if (error != expectedError) {
        char message[64];
        snprintf(message, sizeof(message), "error %u, expected %u",
                 error, expectedError);
        fail(expression, message);
    }
    return value;
}

static void nearValue(const char *expression, long double expected,
                      long double tolerance)
{
    CalcNumber value = evaluate(expression, CALC_OK);
    long double actual = calcNumberToLongDouble(value);
    if (tolerance == 0.0L)
        tolerance = 1e-12L;
    if (fabsl(actual - expected) > tolerance) {
        char message[96];
        snprintf(message, sizeof(message), "got %.12g, expected %.12g",
                 (double)actual, (double)expected);
        fail(expression, message);
    }
}

static void expectFormat(const char *expression, const char *expected)
{
    CalcNumber value = evaluate(expression, CALC_OK);
    char output[40];
    calcFormatNumber(value, output, sizeof(output));
    if (strcmp(output, expected) != 0)
        fail(expression, output);
}

int main(void)
{
    CalcSyntaxGraphRow row;
    CalcNumber x;
    CalcNumber value;
    char inside[64];
    char args[4][32];
    s32 fixed;
    u8 count;
    u8 whole;
    u8 error;

    calcContextInit(&context);
    nearValue("2+3", 5.0L, 0.0L);
    nearValue("Ans*2", 10.0L, 0.0L);
    nearValue("PreAns", 5.0L, 0.0L);
    nearValue("1/2", 0.5L, 0.0L);
    nearValue("-1/2", -0.5L, 0.0L);
    nearValue("1,25+2,75", 4.0L, 0.0L);
    nearValue("-2.5*4", -10.0L, 0.0L);
    nearValue("2(3+4)", 14.0L, 0.0L);
    nearValue("2^10", 1024.0L, 2e-7L);
    nearValue("1E100", 1.0e100L, 1.0e91L);
    nearValue("1E-100", 1.0e-100L, 1.0e-109L);

    x = calcNumberFromFixed(2 * CALC_ONE);
    calcContextSetVariable(&context, 'Y',
                           calcNumberFromFixed(3 * CALC_ONE));
    calcEvalNumberContext(&context, "xy", x, &value, &error);
    if (error != CALC_OK || fabsl(calcNumberToLongDouble(value) - 6.0L) >
        1e-9L)
        fail("xy", "implicit variables");

    nearValue("sin(pi/2)", 1.0L, 2e-8L);
    nearValue("cos(0)", 1.0L, 2e-8L);
    nearValue("tan(pi/4)", 1.0L, 3e-8L);
    nearValue("asin(1)", 1.5707963268L, 3e-8L);
    nearValue("sin^-1(1)", 1.5707963268L, 3e-8L);
    nearValue("acos(1)", 0.0L, 1e-12L);
    nearValue("atan(1)", 0.7853981634L, 3e-8L);
    nearValue("sinh(1)", 1.1752011936L, 3e-8L);
    nearValue("cosh(1)", 1.5430806348L, 3e-8L);
    nearValue("tanh(1)", 0.7615941560L, 3e-8L);
    nearValue("asinh(1)", 0.8813735870L, 3e-8L);
    nearValue("acosh(2)", 1.3169578970L, 3e-8L);
    nearValue("atanh(0,5)", 0.5493061443L, 3e-8L);

    nearValue("sqrt(2)", 1.4142135624L, 3e-8L);
    nearValue("ln(e)", 1.0L, 3e-8L);
    nearValue("log(100)", 2.0L, 3e-8L);
    nearValue("logab(8,2)", 3.0L, 3e-8L);
    nearValue("root(3,8)", 2.0L, 3e-8L);
    nearValue("cbrt(8)", 2.0L, 3e-8L);
    nearValue("pow(2,3)", 8.0L, 3e-8L);
    nearValue("nroot(3,8)", 2.0L, 3e-8L);
    nearValue("log10(1000)", 3.0L, 3e-8L);
    nearValue("lg(1000)", 3.0L, 3e-8L);
    nearValue("exp(1)", 2.718281828L, 4e-8L);

    nearValue("5!", 120.0L, 0.0L);
    nearValue("50%", 0.5L, 0.0L);
    nearValue("nPr(5,2)", 20.0L, 0.0L);
    nearValue("nCr(5,2)", 10.0L, 0.0L);
    nearValue("gcd(28,35)", 7.0L, 0.0L);
    nearValue("lcm(9,15)", 45.0L, 0.0L);
    nearValue("mod(17,5)", 2.0L, 0.0L);
    nearValue("rmdr(17,5)", 2.0L, 0.0L);
    nearValue("intdiv(17,5)", 3.0L, 0.0L);
    nearValue("min(3,5)", 3.0L, 0.0L);
    nearValue("max(3,5)", 5.0L, 0.0L);
    nearValue("hypot(3,4)", 5.0L, 3e-8L);
    nearValue("recip(4)", 0.25L, 0.0L);
    nearValue("sqr(-3)", 9.0L, 0.0L);
    nearValue("sign(-3)", -1.0L, 0.0L);
    nearValue("ceil(-3.5)", -3.0L, 0.0L);
    nearValue("floor(-3.5)", -4.0L, 0.0L);
    nearValue("trunc(-3.5)", -3.0L, 0.0L);
    nearValue("frac(-3.5)", -0.5L, 0.0L);
    nearValue("round(3.4)", 3.0L, 0.0L);
    nearValue("sec(0)", 1.0L, 3e-8L);
    nearValue("csc(pi/2)", 1.0L, 3e-8L);
    nearValue("cot(pi/4)", 1.0L, 3e-8L);
    nearValue("sech(0)", 1.0L, 3e-8L);
    nearValue("csch(ln(2+sqrt(5)))", 0.5L, 4e-8L);

    nearValue("normalpdf(0,0,1)", 0.3989422804L, 3e-8L);
    nearValue("normalcdf(0,0,1)", 0.5L, 3e-8L);
    nearValue("normalinv(0.5,0,1)", 0.0L, 1e-8L);
    nearValue("binompdf(10,0.5,5)", 0.24609375L, 3e-8L);
    nearValue("binomcdf(10,0.5,5)", 0.623046875L, 3e-8L);
    nearValue("poissonpdf(3,2)", 0.1804470443L, 3e-8L);
    nearValue("poissoncdf(3,2)", 0.8571234605L, 3e-8L);

    nearValue("3<4", 1.0L, 0.0L);
    nearValue("3>=4", 0.0L, 0.0L);
    nearValue("3!=4", 1.0L, 0.0L);
    nearValue("RanInt#(2,2)", 2.0L, 0.0L);
    evaluate("Ran#", CALC_OK);

    context.angleMode = CALC_ANGLE_DEG;
    nearValue("tan(45)", 1.0L, 3e-8L);
    nearValue("atan(1)", 45.0L, 3e-7L);
    context.angleMode = CALC_ANGLE_GRAD;
    nearValue("tan(50)", 1.0L, 3e-8L);
    nearValue("atan(1)", 50.0L, 3e-7L);
    calcSetAngleMode(CALC_ANGLE_GRAD);
    if (calcTangentFixed(50 * CALC_ONE, &fixed) != CALC_OK ||
        fixed < CALC_ONE - 1 || fixed > CALC_ONE + 1)
        fail("calcTangentFixed", "unexpected value");
    context.angleMode = CALC_ANGLE_RAD;
    calcSetAngleMode(CALC_ANGLE_RAD);
    if (!calcTangentPhaseCrosses(calcNumberFromFixed(400),
                                 calcNumberFromFixed(405)) ||
        calcTangentPhaseCrosses(calcNumberFromFixed(0),
                                calcNumberFromFixed(100)))
        fail("calcTangentPhaseCrosses", "crossing classification");

    expectFormat("1/3", "0.333333333");
    evaluate("1/0", CALC_ERR_DIVZERO);
    evaluate("sqrt(-1)", CALC_ERR_DOMAIN);
    evaluate("log(0)", CALC_ERR_DOMAIN);
    evaluate("tan(pi/2)", CALC_ERR_DOMAIN);

    if (calcSyntaxParseGraphRow("x>=y^2", &row) != CALC_OK ||
        row.kind != CALC_GRAPH_ROW_INEQUALITY || !row.axisX ||
        strcmp(row.relation, ">=") != 0 || strcmp(row.formulaA, "y^2") != 0)
        fail("x>=y^2", "typed graph row");
    if (calcSyntaxParseGraphRow("param:cos(t);sin(t)", &row) != CALC_OK ||
        row.kind != CALC_GRAPH_ROW_PARAM ||
        strcmp(row.formulaB, "sin(t)") != 0)
        fail("param:cos(t);sin(t)", "typed parametric row");
    if (!calcSyntaxExtractCall("sum(1/(x+1);x;1;10)", "sum", inside,
                               sizeof(inside)) ||
        !calcSyntaxSplitArgs(inside, (char *)args, sizeof(args[0]), 4,
                             &count) || count != 4 ||
        strcmp(args[0], "1/(x+1)") != 0)
        fail("sum(1/(x+1);x;1;10)", "balanced arguments");
    if (!calcSyntaxFindCall("3+tan(x^2)", "tan", inside,
                            sizeof(inside), &whole) || whole ||
        strcmp(inside, "x^2") != 0)
        fail("3+tan(x^2)", "nested call lookup");

    puts("evaluator_catalog_test: PASS");
    return 0;
}
