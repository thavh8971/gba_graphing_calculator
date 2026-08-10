#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "gcalc/graph.h"
#include "gcalc/modes.h"

static void fail(const char *label, const char *detail)
{
    fprintf(stderr, "FAIL %s: %s\n", label, detail);
    exit(1);
}

static void expectMode(ModeRuntime *runtime, CalcMode mode,
                       const char *input, const char *needle)
{
    u8 error = modeEvaluate(runtime, mode, input);
    if (error != CALC_OK)
        fail(input, calcErrorText(error));
    if (strstr(runtime->result, needle) == 0)
        fail(input, runtime->result);
}

static void expectModeExact(ModeRuntime *runtime, CalcMode mode,
                            const char *input, const char *expected)
{
    u8 error = modeEvaluate(runtime, mode, input);
    if (error != CALC_OK)
        fail(input, calcErrorText(error));
    if (strcmp(runtime->result, expected) != 0)
        fail(input, runtime->result);
}

static void expectModeError(ModeRuntime *runtime, CalcMode mode,
                            const char *input, u8 expected)
{
    u8 error = modeEvaluate(runtime, mode, input);
    if (error != expected)
        fail(input, runtime->result);
}

int main(void)
{
    ModeRuntime runtime;
    GraphFunction functions[GRAPH_MAX_FUNCTIONS];
    GraphJob job;
    GraphViewport viewport;
    GraphSample samples[GRAPH_CHUNK_SAMPLES];
    u8 count;
    u8 produced;
    s32 x;
    s32 y;
    s32 x0;
    s32 y0;
    s32 x1;
    s32 y1;

    modeRuntimeInit(&runtime);
    expectMode(&runtime, CALC_MODE_COMP, "sum(x^2;x;1;3)", "14");
    expectMode(&runtime, CALC_MODE_COMP, "prod(x;x;1;4)", "24");
    expectMode(&runtime, CALC_MODE_COMP, "d/dx(x^2;x;3)", "6");

    expectMode(&runtime, CALC_MODE_CMPLX, "conj:3+4i", "3-4i");
    expectMode(&runtime, CALC_MODE_CMPLX, "abs:3+4i", "5");
    expectMode(&runtime, CALC_MODE_CMPLX, "arg:0+1i", "1.570");
    expectMode(&runtime, CALC_MODE_CMPLX, "pow:1+i;2", "2i");

    expectMode(&runtime, CALC_MODE_STAT, "1;2;3;4", "N=4");
    expectMode(&runtime, CALC_MODE_STAT, "170,66;173,68;179,75", "B=");
    expectMode(&runtime, CALC_MODE_STAT, "freq:1,2,3;2,1,1", "M=1.75");
    expectMode(&runtime, CALC_MODE_STAT, "median:1;4;2;3", "2.5");
    expectMode(&runtime, CALC_MODE_STAT, "cuml:1;2;3", "1,3,6");

    expectMode(&runtime, CALC_MODE_BASEN, "2:1011", "DEC=11");
    expectMode(&runtime, CALC_MODE_BASEN, "and:15;7", "DEC=7");
    expectMode(&runtime, CALC_MODE_BASEN, "shl:1;4", "DEC=16");

    expectMode(&runtime, CALC_MODE_EQN, "lin:1,1,3;2,-1,0", "X=1");
    if (strstr(runtime.result, "Y=2") == 0)
        fail("linear system", runtime.result);
    expectMode(&runtime, CALC_MODE_EQN, "poly:1;-3;2", "X1=2");
    expectMode(&runtime, CALC_MODE_EQN, "poly:1;0;-1;0", "X1=-1");
    expectMode(&runtime, CALC_MODE_EQN, "solve:x^2=2;1", "1.414");
    expectMode(&runtime, CALC_MODE_EQN, "solven:x^2=1;-2;2", "-1");
    expectModeExact(&runtime, CALC_MODE_EQN, "x=2", "2");
    expectMode(&runtime, CALC_MODE_EQN,
               "solven:(x-0.1)^2=0;-1;1", "0.1");
    expectModeError(&runtime, CALC_MODE_EQN, "solven:1/x=0;-1;1",
                    CALC_ERR_DOMAIN);
    expectMode(&runtime, CALC_MODE_EQN, "poly:1;-4;6;-4;1", "X1=1");
    expectMode(&runtime, CALC_MODE_EQN,
               "poly:1;0;-14;0;49;0;-36", "X6=3");

    expectMode(&runtime, CALC_MODE_MATRIX, "1,2;3,4", "-2");
    expectMode(&runtime, CALC_MODE_MATRIX, "inv:1,2;3,4", "-0.5");
    expectMode(&runtime, CALC_MODE_MATRIX,
               "det:1,2,3;0,1,4;5,6,0", "1");
    expectMode(&runtime, CALC_MODE_MATRIX,
               "tr:1,2,3;4,5,6;7,8,9", "1,4,7");
    expectMode(&runtime, CALC_MODE_MATRIX,
               "mul:1,2;3,4|4,3;2,1", "8,5");
    expectMode(&runtime, CALC_MODE_MATRIX,
               "mul: [1,2;3,4] | [4,3;2,1] ", "8,5");
    expectModeExact(&runtime, CALC_MODE_MATRIX,
                    "det:[max(1,2),0;0,3]", "6");
    expectModeError(&runtime, CALC_MODE_MATRIX, "det:[1,2;3]",
                    CALC_ERR_SYNTAX);

    expectMode(&runtime, CALC_MODE_TABLE, "x;x^2;0;1;1", "TABLE READY");
    if (runtime.table.functionCount != 2 || runtime.table.rows != 2 ||
        strcmp(runtime.table.z[1], "1") != 0)
        fail("dual table", runtime.result);
    expectMode(&runtime, CALC_MODE_TABLE, "dtable:x^2;0;2;1",
               "TABLE READY");
    if (!runtime.table.hasDerivative ||
        fabsl(strtold(runtime.table.derivative[1], 0) - 2.0L) > 0.001L)
        fail("derivative table", runtime.table.derivative[1]);

    expectMode(&runtime, CALC_MODE_VECTOR, "3,4", "5");
    expectMode(&runtime, CALC_MODE_VECTOR, "cross:1,0,0;0,1,0",
               "[0,0,1]");
    expectMode(&runtime, CALC_MODE_VECTOR, "angle:1,0,0;0,1,0", "1.570");

    expectMode(&runtime, CALC_MODE_INEQ, "2*x<4", "X<2");
    expectMode(&runtime, CALC_MODE_INEQ, "quad:x^2-4<0", "-2<X<2");
    expectMode(&runtime, CALC_MODE_INEQ, "quad:x^2-4>0",
               "X<-2 OR X>2");
    expectMode(&runtime, CALC_MODE_RATIO, "1.5:2.5", "3:5");
    expectModeExact(&runtime, CALC_MODE_RATIO, "1/3:2/3", "1:2");
    expectModeExact(&runtime, CALC_MODE_RATIO,
                    "0.0000000001:0.0000000002", "1:2");
    expectModeExact(&runtime, CALC_MODE_RATIO, "?:2=3:6", "1");
    expectModeExact(&runtime, CALC_MODE_RATIO, "1:?=3:6", "2");
    expectModeExact(&runtime, CALC_MODE_RATIO, "1:2=?:6", "3");
    expectModeExact(&runtime, CALC_MODE_RATIO, "1:2=3:?", "6");
    expectModeError(&runtime, CALC_MODE_RATIO, "1:?=0:6",
                    CALC_ERR_DIVZERO);

    expectMode(&runtime, CALC_MODE_DIST, "10;0.5;5", "0.246");
    expectMode(&runtime, CALC_MODE_DIST, "normal:0;0;1", "0.5");
    expectMode(&runtime, CALC_MODE_DIST, "normalpdf:0;0;1", "0.398");
    expectMode(&runtime, CALC_MODE_DIST, "normalinv:0.5;0;1", "0");
    expectMode(&runtime, CALC_MODE_DIST, "binomcdf:10;0.5;5", "0.623");
    expectMode(&runtime, CALC_MODE_DIST, "poissoncdf:3;2", "0.857");
    expectMode(&runtime, CALC_MODE_DIST, "poissonpdf:3;2", "0.180");
    expectModeError(&runtime, CALC_MODE_DIST, "poissonpdf:2.5;2",
                    CALC_ERR_DOMAIN);
    expectModeError(&runtime, CALC_MODE_DIST, "binompdf:10.5;0.5;2",
                    CALC_ERR_DOMAIN);
    expectMode(&runtime, CALC_MODE_DIST, "geompdf:3;0.5", "0.125");
    expectMode(&runtime, CALC_MODE_DIST, "hypergeom:20;7;5;2", "0.387");

    if (graphParseFunctions("x^2;sin(x)", functions,
                            GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
        count != 2)
        fail("multi graph parse", "wrong count");
    if (graphParseFunctions("param:cos(t);sin(t)", functions,
                            GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
        count != 1 || functions[0].row.kind != CALC_GRAPH_ROW_PARAM)
        fail("parametric graph parse", "wrong row");
    if (graphEvaluatePoint(&functions[0], &runtime.calc, 0, &x, &y) !=
            GRAPH_SAMPLE_VALID || x < CALC_ONE - 1 || y != 0)
        fail("parametric point", "unexpected coordinate");
    if (graphParseFunctions("  param:cos(t);sin(t) ; y=t", functions,
                            GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
        count != 2 || functions[0].row.kind != CALC_GRAPH_ROW_PARAM)
        fail("spaced parametric graph parse", "wrong rows");

    if (graphParseFunctions("x>=y^2", functions,
                            GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
        !functions[0].row.axisX ||
        strcmp(functions[0].row.relation, ">=") != 0)
        fail("inequality graph", "classification");
    if (graphEvaluatePoint(&functions[0], &runtime.calc, CALC_ONE,
                           &x, &y) != GRAPH_SAMPLE_VALID ||
        x != CALC_ONE || y != CALC_ONE)
        fail("inverse inequality point", "unexpected coordinate");

    if (graphParseFunctions("1/(x-0.1)", functions,
                            GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
        count != 1 || !functions[0].needsBridge)
        fail("bridge graph classification", "missing guard");

    {
        CalcNumber answer;
        CalcNumber after;
        u8 error;
        calcEvaluateContext(&runtime.calc, "7", &answer, &error);
        if (error != CALC_OK ||
            graphParseFunctions("Ans+x", functions,
                                GRAPH_MAX_FUNCTIONS, &count) != CALC_OK ||
            graphEvaluatePoint(&functions[0], &runtime.calc, CALC_ONE,
                               &x, &y) != GRAPH_SAMPLE_VALID ||
            y != 8 * CALC_ONE ||
            calcContextGetVariable(&runtime.calc, 'X', &after) != 0)
            fail("graph Answer preservation", "evaluation failed");
        if (!runtime.calc.hasAnswer ||
            runtime.calc.answer.mantissa != answer.mantissa ||
            runtime.calc.answer.exponent != answer.exponent)
            fail("graph Answer preservation", "Answer was mutated");
    }

    viewport.xMin = -2 * CALC_ONE;
    viewport.xMax = 2 * CALC_ONE;
    viewport.yMin = -2 * CALC_ONE;
    viewport.yMax = 2 * CALC_ONE;
    graphJobBegin(&job, &runtime.calc, functions, count, viewport, 241);
    if (graphJobStep(&job, samples, GRAPH_CHUNK_SAMPLES, &produced) !=
            CALC_OK || produced != GRAPH_CHUNK_SAMPLES)
        fail("streaming graph", "wrong chunk size");

    if (!graphClipWorldSegment(-2 * CALC_ONE, -2 * CALC_ONE,
                               2 * CALC_ONE, 2 * CALC_ONE,
                               -CALC_ONE, CALC_ONE,
                               -CALC_ONE, CALC_ONE,
                               &x0, &y0, &x1, &y1) ||
        x0 != -CALC_ONE || y0 != -CALC_ONE ||
        x1 != CALC_ONE || y1 != CALC_ONE)
        fail("world clipping", "wrong endpoints");
    if (graphClipWorldSegment(-3 * CALC_ONE, -3 * CALC_ONE,
                              -2 * CALC_ONE, -2 * CALC_ONE,
                              -CALC_ONE, CALC_ONE,
                              -CALC_ONE, CALC_ONE,
                              &x0, &y0, &x1, &y1))
        fail("world clipping rejection", "accepted outside segment");
    if (!graphClipWorldSegment(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX,
                               -1, 1, -1, 1, &x0, &y0, &x1, &y1) ||
        x0 != -1 || y0 != -1 || x1 != 1 || y1 != 1)
        fail("extreme world clipping", "overflow or wrong endpoints");

    puts("modes_graph_test: PASS");
    return 0;
}
