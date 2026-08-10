#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gcalc/graph.h"
#include "gcalc/modes.h"

static int failures;

static void fail(const char *contract, const char *detail)
{
    fprintf(stderr, "FAIL mode contract: %s: %s\n", contract, detail);
    failures++;
}

static void expectAlias(CalcMode mode, const char *canonical,
                        const char *legacy)
{
    ModeRuntime canonicalRuntime;
    ModeRuntime legacyRuntime;
    u8 canonicalError;
    u8 legacyError;

    modeRuntimeInit(&canonicalRuntime);
    modeRuntimeInit(&legacyRuntime);
    canonicalError = modeEvaluate(&canonicalRuntime, mode, canonical);
    legacyError = modeEvaluate(&legacyRuntime, mode, legacy);
    if (canonicalError != CALC_OK) {
        fail(canonical, canonicalRuntime.result);
        return;
    }
    if (legacyError != CALC_OK) {
        fail(legacy, legacyRuntime.result);
        return;
    }
    if (strcmp(canonicalRuntime.result, legacyRuntime.result) != 0) {
        char detail[MODE_RESULT_CAPACITY * 2 + 32];
        snprintf(detail, sizeof(detail), "canonical='%s', legacy='%s'",
                 canonicalRuntime.result, legacyRuntime.result);
        fail(canonical, detail);
    }
    if (mode == CALC_MODE_TABLE &&
        memcmp(&canonicalRuntime.table, &legacyRuntime.table,
               sizeof(canonicalRuntime.table)) != 0)
        fail(canonical, "canonical and legacy table payloads differ");
}

static void expectModeOk(CalcMode mode, const char *expression,
                         const char *needle)
{
    ModeRuntime runtime;
    u8 error;

    modeRuntimeInit(&runtime);
    error = modeEvaluate(&runtime, mode, expression);
    if (error != CALC_OK) {
        fail(expression, runtime.result);
        return;
    }
    if (needle != 0 && strstr(runtime.result, needle) == 0)
        fail(expression, runtime.result);
}

static void expectModeRejected(CalcMode mode, const char *expression)
{
    ModeRuntime runtime;

    modeRuntimeInit(&runtime);
    if (modeEvaluate(&runtime, mode, expression) == CALC_OK)
        fail(expression, "accepted by a mode that does not own the command");
}

static void testCanonicalCallCompatibility(void)
{
    /* CMPLX keeps arithmetic input while exposing complex-only calls. */
    expectAlias(CALC_MODE_CMPLX, "conj(3+4i)", "conj:3+4i");
    expectAlias(CALC_MODE_CMPLX, "re(3+4i)", "re:3+4i");
    expectAlias(CALC_MODE_CMPLX, "im(3+4i)", "im:3+4i");
    expectAlias(CALC_MODE_CMPLX, "abs(3+4i)", "abs:3+4i");
    expectAlias(CALC_MODE_CMPLX, "norm(3+4i)", "norm:3+4i");
    expectAlias(CALC_MODE_CMPLX, "arg(0+1i)", "arg:0+1i");
    expectAlias(CALC_MODE_CMPLX, "polar(5;0)", "polar:5;0");
    expectAlias(CALC_MODE_CMPLX, "rect(3;4)", "rect:3;4");
    expectAlias(CALC_MODE_CMPLX, "pow(1+i;2)", "pow:1+i;2");

    /* STAT command calls supersede colon prefixes without deleting saves. */
    expectAlias(CALC_MODE_STAT, "statfreq(1,2,3;2,1,1)",
                "freq:1,2,3;2,1,1");
    expectAlias(CALC_MODE_STAT, "sum(1;2;3)", "sum:1;2;3");
    expectAlias(CALC_MODE_STAT, "prod(1;2;3)", "prod:1;2;3");
    expectAlias(CALC_MODE_STAT, "mean(1;2;3)", "mean:1;2;3");
    expectAlias(CALC_MODE_STAT, "sumx2(1;2;3)", "sumx2:1;2;3");
    expectAlias(CALC_MODE_STAT, "median(1;4;2;3)",
                "median:1;4;2;3");
    expectAlias(CALC_MODE_STAT, "varp(1;2;3)", "varp:1;2;3");
    expectAlias(CALC_MODE_STAT, "vars(1;2;3)", "vars:1;2;3");
    expectAlias(CALC_MODE_STAT, "sdpop(1;2;3)", "sdpop:1;2;3");
    expectAlias(CALC_MODE_STAT, "sdsamp(1;2;3)", "sdsamp:1;2;3");
    expectAlias(CALC_MODE_STAT, "cuml(1;2;3)", "cuml:1;2;3");

    /* BASE-N names describe the source radix; operations are 32-bit. */
    expectAlias(CALC_MODE_BASEN, "bin(1011)", "2:1011");
    expectAlias(CALC_MODE_BASEN, "oct(17)", "8:17");
    expectAlias(CALC_MODE_BASEN, "dec(15)", "10:15");
    expectAlias(CALC_MODE_BASEN, "hex(FF)", "16:FF");
    expectAlias(CALC_MODE_BASEN, "and(15;7)", "and:15;7");
    expectAlias(CALC_MODE_BASEN, "or(8;7)", "or:8;7");
    expectAlias(CALC_MODE_BASEN, "xor(15;7)", "xor:15;7");
    expectAlias(CALC_MODE_BASEN, "xnor(15;7)", "xnor:15;7");
    expectAlias(CALC_MODE_BASEN, "not(0)", "not:0");
    expectAlias(CALC_MODE_BASEN, "neg(1)", "neg:1");
    expectAlias(CALC_MODE_BASEN, "shl(1;4)", "shl:1;4");
    expectAlias(CALC_MODE_BASEN, "shr(16;4)", "shr:16;4");

    expectAlias(CALC_MODE_EQN, "lin(1,1,3;2,-1,0)",
                "lin:1,1,3;2,-1,0");
    expectAlias(CALC_MODE_EQN, "poly(1;-3;2)", "poly:1;-3;2");
    expectAlias(CALC_MODE_EQN, "solve(x^2=2;1)", "solve:x^2=2;1");
    expectAlias(CALC_MODE_EQN, "solven(x^2=1;-2;2)",
                "solven:x^2=1;-2;2");

    expectAlias(CALC_MODE_MATRIX, "det([1,2;3,4])",
                "det:[1,2;3,4]");
    expectAlias(CALC_MODE_MATRIX, "inv([1,2;3,4])",
                "inv:[1,2;3,4]");
    expectAlias(CALC_MODE_MATRIX, "tr([1,2;3,4])",
                "tr:[1,2;3,4]");
    expectAlias(CALC_MODE_MATRIX, "transpose([1,2;3,4])",
                "transpose:[1,2;3,4]");
    expectAlias(CALC_MODE_MATRIX, "add([1,2;3,4]|[4,3;2,1])",
                "add:[1,2;3,4]|[4,3;2,1]");
    expectAlias(CALC_MODE_MATRIX, "sub([1,2;3,4]|[4,3;2,1])",
                "sub:[1,2;3,4]|[4,3;2,1]");
    expectAlias(CALC_MODE_MATRIX, "mul([1,2;3,4]|[4,3;2,1])",
                "mul:[1,2;3,4]|[4,3;2,1]");

    expectAlias(CALC_MODE_TABLE, "dtable(x^2;0;2;1)",
                "dtable:x^2;0;2;1");

    expectAlias(CALC_MODE_VECTOR, "norm(3,4)", "norm:3,4");
    expectAlias(CALC_MODE_VECTOR, "scale(2;3,4)", "scale:2;3,4");
    expectAlias(CALC_MODE_VECTOR, "dot(1,2;3,4)", "dot:1,2;3,4");
    expectAlias(CALC_MODE_VECTOR, "cross(1,0,0;0,1,0)",
                "cross:1,0,0;0,1,0");
    expectAlias(CALC_MODE_VECTOR, "angle(1,0;0,1)",
                "angle:1,0;0,1");

    expectAlias(CALC_MODE_INEQ, "quad(x^2-4<0)", "quad:x^2-4<0");
    expectAlias(CALC_MODE_INEQ, "ineq2(1;0;-4;<)",
                "ineq2:1;0;-4;<");
    expectAlias(CALC_MODE_INEQ, "ineq3(1;0;0;0;<)",
                "ineq3:1;0;0;0;<");
    expectAlias(CALC_MODE_INEQ, "ineq4(1;0;0;0;-1;<)",
                "ineq4:1;0;0;0;-1;<");

    /* fx-580VN X call order is x;n;p; old compact saves used n;p;x. */
    expectAlias(CALC_MODE_DIST, "binompdf(5;10;0.5)",
                "binompdf:10;0.5;5");
    expectAlias(CALC_MODE_DIST, "binomcdf(5;10;0.5)",
                "binomcdf:10;0.5;5");
    expectAlias(CALC_MODE_DIST, "poissonpdf(3;2)", "poissonpdf:3;2");
    expectAlias(CALC_MODE_DIST, "poissoncdf(3;2)", "poissoncdf:3;2");
    /* Canonical fx-580VN X normal calls use sigma before mu.  The older
       normal* spelling/order remains a compatibility surface. */
    expectAlias(CALC_MODE_DIST, "normpdf(0;1;0)", "normalpdf:0;0;1");
    expectAlias(CALC_MODE_DIST, "normcdf(-100;0;1;0)",
                "normalcdf:0;0;1");
    expectAlias(CALC_MODE_DIST, "norminv(0.5;1;0)",
                "normalinv:0.5;0;1");
    expectAlias(CALC_MODE_DIST, "normalpdf(0;0;1)",
                "normalpdf:0;0;1");
    expectAlias(CALC_MODE_DIST, "normalcdf(0;0;1)",
                "normalcdf:0;0;1");
    expectAlias(CALC_MODE_DIST, "normalinv(0.5;0;1)",
                "normalinv:0.5;0;1");
    expectAlias(CALC_MODE_DIST, "geompdf(3;0.5)", "geompdf:3;0.5");
    expectAlias(CALC_MODE_DIST, "geomcdf(3;0.5)", "geomcdf:3;0.5");
    expectAlias(CALC_MODE_DIST, "hypergeom(20;7;5;2)",
                "hypergeom:20;7;5;2");
}

static void testModeOwnership(void)
{
    /* COMP owns ordinary calculation and its CALC/SOLVE conveniences. */
    expectModeOk(CALC_MODE_COMP, "1+2*3", "7");
    expectModeOk(CALC_MODE_COMP, "calc(2+3)", "5");
    expectModeOk(CALC_MODE_COMP, "solve(x^2=4;1)", "1.999");

    /* CMPLX is COMP plus complex math, not a shortcut to every mode. */
    expectModeOk(CALC_MODE_CMPLX, "1+2", "3");
    expectModeOk(CALC_MODE_CMPLX, "conj(3+4i)", "3-4i");

    expectModeRejected(CALC_MODE_COMP, "conj(3+4i)");
    expectModeRejected(CALC_MODE_COMP, "det([1,2;3,4])");
    expectModeRejected(CALC_MODE_COMP, "binompdf(10;0.5;5)");
    expectModeRejected(CALC_MODE_CMPLX, "det([1,2;3,4])");
    expectModeRejected(CALC_MODE_STAT, "det([1,2;3,4])");
    expectModeRejected(CALC_MODE_BASEN, "sin(1)");
    expectModeRejected(CALC_MODE_MATRIX, "binompdf(10;0.5;5)");
    expectModeRejected(CALC_MODE_VECTOR, "solve(x=1;0)");
    expectModeRejected(CALC_MODE_DIST, "det([1,2;3,4])");
}

static void testGraphCanonicalRowCompatibility(void)
{
    GraphFunction canonical[GRAPH_MAX_FUNCTIONS];
    GraphFunction legacy[GRAPH_MAX_FUNCTIONS];
    u8 canonicalCount = 0;
    u8 legacyCount = 0;
    u8 canonicalError;
    u8 legacyError;

    canonicalError = graphParseFunctions("param(cos(t);sin(t))", canonical,
                                         GRAPH_MAX_FUNCTIONS,
                                         &canonicalCount);
    legacyError = graphParseFunctions("param:cos(t);sin(t)", legacy,
                                      GRAPH_MAX_FUNCTIONS, &legacyCount);
    if (canonicalError != CALC_OK || legacyError != CALC_OK ||
        canonicalCount != 1 || legacyCount != 1)
        fail("param(...) graph row", "canonical or legacy row did not parse");
    else if (canonical[0].row.kind != CALC_GRAPH_ROW_PARAM ||
             legacy[0].row.kind != CALC_GRAPH_ROW_PARAM ||
             strcmp(canonical[0].row.formulaA, legacy[0].row.formulaA) != 0 ||
             strcmp(canonical[0].row.formulaB, legacy[0].row.formulaB) != 0)
        fail("param(...) graph row", "canonical and legacy rows differ");
}

int main(void)
{
    testCanonicalCallCompatibility();
    testModeOwnership();
    testGraphCanonicalRowCompatibility();

    if (failures != 0) {
        fprintf(stderr, "mode_command_contract_test: %d failure(s)\n",
                failures);
        return 1;
    }
    puts("mode_command_contract_test: PASS");
    return 0;
}
