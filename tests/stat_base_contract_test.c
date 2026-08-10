#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "gcalc/modes.h"

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL STAT/BASE contract: %s\n", message);
        failures++;
    }
}

static u8 evaluate(ModeRuntime *runtime, CalcMode mode,
                   const char *expression)
{
    u8 error = modeEvaluate(runtime, mode, expression);
    if (error != CALC_OK)
        fprintf(stderr, "DETAIL %s -> %s\n", expression, runtime->result);
    return error;
}

static void expectResult(CalcMode mode, const char *expression,
                         const char *needle, const char *message)
{
    ModeRuntime runtime;
    modeRuntimeInit(&runtime);
    check(evaluate(&runtime, mode, expression) == CALC_OK, message);
    if (needle != 0)
        check(strstr(runtime.result, needle) != 0, message);
}

static void expectError(CalcMode mode, const char *expression,
                        u8 expected, const char *message)
{
    ModeRuntime runtime;
    modeRuntimeInit(&runtime);
    check(modeEvaluate(&runtime, mode, expression) == expected, message);
}

static void expectSameStat(const char *weighted, const char *expanded,
                           const char *message)
{
    ModeRuntime weightedRuntime;
    ModeRuntime expandedRuntime;
    modeRuntimeInit(&weightedRuntime);
    modeRuntimeInit(&expandedRuntime);
    check(evaluate(&weightedRuntime, CALC_MODE_STAT, weighted) == CALC_OK,
          message);
    check(evaluate(&expandedRuntime, CALC_MODE_STAT, expanded) == CALC_OK,
          message);
    check(strcmp(weightedRuntime.result, expandedRuntime.result) == 0,
          message);
}

static void testStatModelApi(void)
{
    static const char *const expectedLabels[STAT_MODEL_COUNT] = {
        "1-VAR", "A+BX", "A+BX+CX2", "A+B lnX",
        "A e^(BX)", "A B^X", "A X^B", "A+B/X"
    };
    ModeRuntime runtime;
    u8 model;

    modeRuntimeInit(&runtime);
    check(modeGetStatModel(&runtime) == STAT_MODEL_1VAR,
          "STAT defaults to the 1-VAR selector item");
    for (model = 0; model < STAT_MODEL_COUNT; model++) {
        modeSetStatModel(&runtime, (StatModel)model);
        check(modeGetStatModel(&runtime) == (StatModel)model,
              "every STAT selector item persists in runtime state");
        check(strcmp(modeStatModelLabel((StatModel)model),
                     expectedLabels[model]) == 0,
              "STAT selector labels follow the eight-type contract");
    }
    modeSetStatModel(&runtime, (StatModel)STAT_MODEL_COUNT);
    check(modeGetStatModel(&runtime) == STAT_MODEL_INVERSE,
          "invalid STAT selector values do not corrupt current state");
    check(strcmp(modeStatModelLabel((StatModel)STAT_MODEL_COUNT), "?") == 0,
          "invalid STAT selector labels are bounded");
}

static void testStatColumnContracts(void)
{
    /* 1-VAR owns X/Freq; regression owns X/Y with optional Freq. */
    expectResult(CALC_MODE_STAT, "stat1var(1;2;3)", "N=3",
                 "1-VAR accepts an X column");
    expectResult(CALC_MODE_STAT, "statfreq(1,2,3;2,1,1)", "N=4",
                 "1-VAR accepts matching X/Freq columns");
    expectError(CALC_MODE_STAT, "statfreq(1,2;1)", CALC_ERR_SYNTAX,
                "1-VAR rejects mismatched X/Freq column lengths");
    expectError(CALC_MODE_STAT, "statfreq(1,2;1,-1)", CALC_ERR_DOMAIN,
                "1-VAR rejects negative frequency");
    expectError(CALC_MODE_STAT, "statfreq(1,2;1,1.5)", CALC_ERR_DOMAIN,
                "1-VAR frequency cells are nonnegative integers");
    expectError(CALC_MODE_STAT, "statfreq(1,2;0,0)", CALC_ERR_DOMAIN,
                "1-VAR requires positive total frequency");

    expectResult(CALC_MODE_STAT, "linear(1,3;2,5;3,7)", "A=1",
                 "paired STAT accepts X/Y rows");
    expectResult(CALC_MODE_STAT, "linear(1,3,2;2,5,1;3,7,1)", "N=4",
                 "paired STAT accepts X/Y/Freq rows");
    expectError(CALC_MODE_STAT, "linear(1;2;3)", CALC_ERR_SYNTAX,
                "paired STAT rejects rows without Y");
    expectError(CALC_MODE_STAT, "linear(1,3,-1;2,5,1)",
                CALC_ERR_DOMAIN,
                "paired STAT rejects negative frequency");
    expectError(CALC_MODE_STAT, "linear(1,3,1.5;2,5,1)",
                CALC_ERR_DOMAIN,
                "paired STAT frequency cells are integers");
}

static void testAllStatCalculations(void)
{
    ModeRuntime runtime;
    static const struct StatCase {
        StatModel model;
        const char *call;
        const char *raw;
        const char *a;
        const char *b;
        const char *c;
    } cases[] = {
        {STAT_MODEL_LINEAR,
         "linear(1,5;2,8;3,11;4,14)",
         "1,5;2,8;3,11;4,14", "A=2", "B=3", 0},
        {STAT_MODEL_QUADRATIC,
         "quadratic(0,1;1,6;2,17;3,34)",
         "0,1;1,6;2,17;3,34", "A=1", "B=2", "C=3"},
        {STAT_MODEL_LOGARITHMIC,
         "logarithmic(1,2;2.718281828,5;7.389056099,8)",
         "1,2;2.718281828,5;7.389056099,8", "A=2", "B=3", 0},
        {STAT_MODEL_EXPONENTIAL,
         "exponential(0,2;1,5.436563657;2,14.7781122)",
         "0,2;1,5.436563657;2,14.7781122", "A=2", "B=1", 0},
        {STAT_MODEL_AB_EXPONENTIAL,
         "abexp(0,2;1,6;2,18)",
         "0,2;1,6;2,18", "A=2", "B=3", 0},
        {STAT_MODEL_POWER,
         "power(1,2;2,8;4,32)",
         "1,2;2,8;4,32", "A=2", "B=2", 0},
        {STAT_MODEL_INVERSE,
         "inverse(1,5;2,3;4,2)",
         "1,5;2,3;4,2", "A=1", "B=4", 0}
    };
    u8 index;

    for (index = 0; index < GCALC_ARRAY_COUNT(cases); index++) {
        modeRuntimeInit(&runtime);
        check(evaluate(&runtime, CALC_MODE_STAT, cases[index].call) ==
              CALC_OK, "explicit STAT model call evaluates");
        check(strstr(runtime.result, cases[index].a) != 0 &&
              strstr(runtime.result, cases[index].b) != 0,
              "STAT regression returns expected A/B coefficients");
        if (cases[index].c != 0)
            check(strstr(runtime.result, cases[index].c) != 0,
                  "quadratic STAT returns the C coefficient");
        check(modeGetStatModel(&runtime) == cases[index].model,
              "explicit STAT model call updates selector state");

        modeRuntimeInit(&runtime);
        modeSetStatModel(&runtime, cases[index].model);
        check(evaluate(&runtime, CALC_MODE_STAT, cases[index].raw) ==
              CALC_OK, "selected STAT model evaluates serialized table rows");
        check(strstr(runtime.result, cases[index].a) != 0 &&
              strstr(runtime.result, cases[index].b) != 0,
              "selected STAT model and explicit call agree");
    }

    expectError(CALC_MODE_STAT, "logarithmic(0,1;1,2)",
                CALC_ERR_DOMAIN, "logarithmic regression requires X > 0");
    expectError(CALC_MODE_STAT, "exponential(0,0;1,2)",
                CALC_ERR_DOMAIN, "e^X regression requires Y > 0");
    expectError(CALC_MODE_STAT, "abexp(0,-1;1,2)",
                CALC_ERR_DOMAIN, "a*b^X regression requires Y > 0");
    expectError(CALC_MODE_STAT, "power(0,1;1,2)",
                CALC_ERR_DOMAIN, "power regression requires X and Y > 0");
    expectError(CALC_MODE_STAT, "inverse(0,1;1,2)",
                CALC_ERR_DOMAIN, "inverse regression rejects X = 0");
}

static void testWeightedKernelParity(void)
{
    /* These compare sufficient-sum kernels with literal row expansion. */
    expectSameStat("statfreq(1,2,4;3,2,1)",
                   "stat1var(1;1;1;2;2;4)",
                   "1-VAR weighted kernel matches expanded rows");
    expectSameStat("linear(1,3,3;2,5,2;4,9,1)",
                   "linear(1,3;1,3;1,3;2,5;2,5;4,9)",
                   "linear weighted kernel matches expanded rows");
    expectSameStat("quadratic(0,1,2;1,6,3;2,17,1;3,34,2)",
                   "quadratic(0,1;0,1;1,6;1,6;1,6;2,17;3,34;3,34)",
                   "quadratic weighted kernel matches expanded rows");
    expectSameStat("logarithmic(1,2,2;2.718281828,5,3;7.389056099,8,1)",
                   "logarithmic(1,2;1,2;2.718281828,5;2.718281828,5;"
                   "2.718281828,5;7.389056099,8)",
                   "logarithmic weighted kernel matches expanded rows");
    expectSameStat("exponential(0,2,2;1,5.436563657,2;2,14.7781122,1)",
                   "exponential(0,2;0,2;1,5.436563657;1,5.436563657;"
                   "2,14.7781122)",
                   "exponential weighted kernel matches expanded rows");
    expectSameStat("abexp(0,2,2;1,6,2;2,18,1)",
                   "abexp(0,2;0,2;1,6;1,6;2,18)",
                   "a*b^X weighted kernel matches expanded rows");
    expectSameStat("power(1,2,2;2,8,2;4,32,1)",
                   "power(1,2;1,2;2,8;2,8;4,32)",
                   "power weighted kernel matches expanded rows");
    expectSameStat("inverse(1,5,2;2,3,2;4,2,1)",
                   "inverse(1,5;1,5;2,3;2,3;4,2)",
                   "inverse weighted kernel matches expanded rows");
}

static void testBaseRadixApi(void)
{
    static const struct DigitCase {
        BaseRadix radix;
        const char *valid;
        const char *invalid;
    } cases[] = {
        {BASE_RADIX_BIN, "01", "23456789ABCDEF"},
        {BASE_RADIX_OCT, "01234567", "89ABCDEF"},
        {BASE_RADIX_DEC, "0123456789", "ABCDEF"},
        {BASE_RADIX_HEX, "0123456789ABCDEFabcdef", "GZ"}
    };
    ModeRuntime runtime;
    char buffer[40];
    u8 index;

    modeRuntimeInit(&runtime);
    check(modeGetBaseRadix(&runtime) == BASE_RADIX_DEC,
          "BASE-N defaults to DEC");
    check(strcmp(modeBaseRadixLabel(BASE_RADIX_DEC), "DEC") == 0,
          "BASE-N exposes the DEC label");
    for (index = 0; index < GCALC_ARRAY_COUNT(cases); index++) {
        const char *digit;
        check(modeSetBaseRadix(&runtime, cases[index].radix),
              "every BASE-N radix can be selected");
        check(modeGetBaseRadix(&runtime) == cases[index].radix,
              "selected BASE-N radix persists");
        for (digit = cases[index].valid; *digit != '\0'; digit++)
            check(modeBaseDigitValid(cases[index].radix, *digit),
                  "valid BASE-N keypad digit is enabled");
        for (digit = cases[index].invalid; *digit != '\0'; digit++)
            check(!modeBaseDigitValid(cases[index].radix, *digit),
                  "invalid BASE-N keypad digit is disabled");
    }
    check(!modeSetBaseRadix(&runtime, (BaseRadix)3),
          "unsupported radices cannot enter runtime state");
    check(modeGetBaseRadix(&runtime) == BASE_RADIX_HEX,
          "invalid radix selection preserves the previous radix");
    check(strcmp(modeBaseRadixLabel((BaseRadix)3), "?") == 0,
          "unsupported radix labels are bounded");
    check(!modeBaseDigitValid((BaseRadix)3, '2'),
          "unsupported radices enable no digits");

    check(modeBaseFormatValue(BASE_RADIX_BIN, -1, buffer,
                              sizeof(buffer)) &&
          strcmp(buffer, "11111111111111111111111111111111") == 0,
          "BIN formats negative values as 32-bit two's complement");
    check(modeBaseFormatValue(BASE_RADIX_OCT, -1, buffer,
                              sizeof(buffer)) &&
          strcmp(buffer, "37777777777") == 0,
          "OCT formats negative values as 32-bit two's complement");
    check(modeBaseFormatValue(BASE_RADIX_HEX, -1, buffer,
                              sizeof(buffer)) &&
          strcmp(buffer, "FFFFFFFF") == 0,
          "HEX formats negative values as 32-bit two's complement");
    check(modeBaseFormatValue(BASE_RADIX_HEX, INT_MIN, buffer,
                              sizeof(buffer)) &&
          strcmp(buffer, "80000000") == 0,
          "BASE-N preserves the INT32_MIN bit pattern");
    check(!modeBaseFormatValue(BASE_RADIX_BIN, -1, buffer, 32),
          "BASE-N formatting reports insufficient destination capacity");
}

static void testBaseEvaluation(void)
{
    ModeRuntime runtime;

    modeRuntimeInit(&runtime);
    check(evaluate(&runtime, CALC_MODE_BASEN, "10") == CALC_OK &&
          strstr(runtime.result, "DEC=10") != 0,
          "raw BASE-N input starts in decimal");
    check(modeSetBaseRadix(&runtime, BASE_RADIX_BIN),
          "BIN radix selection succeeds");
    check(evaluate(&runtime, CALC_MODE_BASEN, "10") == CALC_OK &&
          strstr(runtime.result, "DEC=2") != 0,
          "raw BASE-N input uses selected radix");
    check(modeEvaluate(&runtime, CALC_MODE_BASEN, "2") ==
          CALC_ERR_SYNTAX,
          "BIN rejects digit 2 at evaluation boundary");
    check(evaluate(&runtime, CALC_MODE_BASEN, "hex(FF)") == CALC_OK &&
          modeGetBaseRadix(&runtime) == BASE_RADIX_HEX &&
          strstr(runtime.result, "DEC=255") != 0,
          "hex() converts and selects HEX");
    check(evaluate(&runtime, CALC_MODE_BASEN, "dec(10)") == CALC_OK &&
          modeGetBaseRadix(&runtime) == BASE_RADIX_DEC,
          "dec() converts and selects DEC");

    expectResult(CALC_MODE_BASEN, "not(0)", "DEC=-1",
                 "NOT uses all 32 bits");
    expectResult(CALC_MODE_BASEN, "neg(1)", "DEC=-1",
                 "Neg computes 32-bit two's complement");
    expectResult(CALC_MODE_BASEN, "xnor(15;7)", "DEC=-9",
                 "XNOR complements all 32 result bits");
    expectResult(CALC_MODE_BASEN, "and(hex(FF);bin(1010))", "DEC=10",
                 "BASE-N logical operands may name their own radix");
    expectResult(CALC_MODE_BASEN, "shr(hex(FFFFFFFF);1)",
                 "DEC=2147483647",
                 "BASE-N right shift is logical over 32 bits");
    expectResult(CALC_MODE_BASEN, "shl(1;31)", "DEC=-2147483648",
                 "BASE-N left shift preserves the 32-bit sign bit");

    expectError(CALC_MODE_BASEN, "hex(-1)", CALC_ERR_SYNTAX,
                "non-decimal negative entry uses Neg(), not a minus sign");
    expectError(CALC_MODE_BASEN, "dec(2147483648)", CALC_ERR_SYNTAX,
                "decimal entry is limited to signed 32-bit maximum");
    expectError(CALC_MODE_BASEN, "hex(100000000)", CALC_ERR_SYNTAX,
                "non-decimal entry is limited to 32 bits");
    expectError(CALC_MODE_BASEN, "shl(1;32)", CALC_ERR_RANGE,
                "left shift count is bounded to 0..31");
    expectError(CALC_MODE_BASEN, "shr(1;-1)", CALC_ERR_RANGE,
                "right shift count rejects negative values");
}

static void testBaseArithmetic(void)
{
    ModeRuntime runtime;

    modeRuntimeInit(&runtime);
    check(modeSetBaseRadix(&runtime, BASE_RADIX_BIN),
          "BIN arithmetic radix selection succeeds");
    check(evaluate(&runtime, CALC_MODE_BASEN, "101+1") == CALC_OK &&
          strstr(runtime.result, "BIN=110") != 0 &&
          strstr(runtime.result, "DEC=6") != 0,
          "BIN arithmetic parses operands in the selected radix");
    check(evaluate(&runtime, CALC_MODE_BASEN, "10+1*11") == CALC_OK &&
          strstr(runtime.result, "BIN=101") != 0,
          "BASE-N multiplication binds before addition");
    check(evaluate(&runtime, CALC_MODE_BASEN, "(10+1)*11") == CALC_OK &&
          strstr(runtime.result, "BIN=1001") != 0,
          "BASE-N parentheses override arithmetic precedence");

    check(modeSetBaseRadix(&runtime, BASE_RADIX_HEX),
          "HEX arithmetic radix selection succeeds");
    check(evaluate(&runtime, CALC_MODE_BASEN, "1F+1") == CALC_OK &&
          strstr(runtime.result, "HEX=20") != 0 &&
          strstr(runtime.result, "DEC=32") != 0,
          "HEX arithmetic accepts hexadecimal digits");
    check(evaluate(&runtime, CALC_MODE_BASEN,
                   "hex(10)+bin(10)") == CALC_OK &&
          strstr(runtime.result, "DEC=18") != 0,
          "BASE-N arithmetic accepts explicit-radix subexpressions");

    expectError(CALC_MODE_BASEN, "1/0", CALC_ERR_DIVZERO,
                "BASE-N arithmetic reports division by zero");
    modeRuntimeInit(&runtime);
    expectError(CALC_MODE_BASEN, "2147483647+1", CALC_ERR_RANGE,
                "BASE-N addition reports signed 32-bit overflow");
    expectError(CALC_MODE_BASEN, "50000*50000", CALC_ERR_RANGE,
                "BASE-N multiplication reports signed 32-bit overflow");

    modeSetBaseRadix(&runtime, BASE_RADIX_BIN);
    check(evaluate(&runtime, CALC_MODE_BASEN, "and:15;7") == CALC_OK &&
          strstr(runtime.result, "DEC=7") != 0,
          "legacy BASE-N command arguments retain decimal interpretation");
}

int main(void)
{
    testStatModelApi();
    testStatColumnContracts();
    testAllStatCalculations();
    testWeightedKernelParity();
    testBaseRadixApi();
    testBaseEvaluation();
    testBaseArithmetic();

    if (failures != 0) {
        fprintf(stderr, "stat_base_contract_test: %d failure(s)\n", failures);
        return 1;
    }
    puts("stat_base_contract_test: PASS");
    return 0;
}
