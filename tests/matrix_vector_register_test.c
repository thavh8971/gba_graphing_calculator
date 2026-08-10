#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gcalc/modes.h"

#define MATRIX_LONG_ZERO "000000000000000"
#define MATRIX_LONG_ONE  "000000000000001"

static int failures;

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL named matrix/vector registers: %s\n", message);
        failures++;
    }
}

static void expectResult(ModeRuntime *runtime, CalcMode mode,
                         const char *expression, const char *expected)
{
    u8 error = modeEvaluate(runtime, mode, expression);

    if (error != CALC_OK || strcmp(runtime->result, expected) != 0) {
        fprintf(stderr, "FAIL %s -> error=%u result=%s (expected %s)\n",
                expression, (unsigned)error, runtime->result, expected);
        failures++;
    }
}

static void expectContains(ModeRuntime *runtime, CalcMode mode,
                           const char *expression, const char *expected)
{
    u8 error = modeEvaluate(runtime, mode, expression);

    if (error != CALC_OK || strstr(runtime->result, expected) == 0) {
        fprintf(stderr, "FAIL %s -> error=%u result=%s (missing %s)\n",
                expression, (unsigned)error, runtime->result, expected);
        failures++;
    }
}

static CalcNumber number(long double value)
{
    u8 error;
    CalcNumber result = calcNumberFromLongDouble(value, &error);

    check(error == CALC_OK, "test value converts to CalcNumber");
    return result;
}

static void testMatrixRegisters(void)
{
    ModeRuntime runtime;
    ModeMatrixRegister stored;
    CalcNumber direct[MODE_MATRIX_MAX_ROWS][MODE_MATRIX_MAX_COLUMNS];

    modeRuntimeInit(&runtime);
    memset(direct, 0, sizeof(direct));
    check(strcmp(modeMatrixRegisterLabel(MODE_REGISTER_A), "MatA") == 0 &&
          strcmp(modeMatrixRegisterLabel(MODE_REGISTER_D), "MatD") == 0,
          "matrix labels are MatA through MatD");
    check(!modeMatrixGetRegister(&runtime, MODE_REGISTER_A, &stored),
          "matrix registers start undefined");
    check(modeMatrixSetRegisterExpression(&runtime, MODE_REGISTER_A,
                                          "[1,2;3,4]") == CALC_OK,
          "MatA accepts canonical inline matrix text");
    check(modeMatrixSetRegisterExpression(&runtime, MODE_REGISTER_B,
                                          "[4,3;2,1]") == CALC_OK,
          "MatB is independent from MatA");
    check(modeMatrixGetRegister(&runtime, MODE_REGISTER_A, &stored) &&
          stored.rows == 2 && stored.columns == 2 && stored.defined &&
          fabsl(calcNumberToLongDouble(stored.cell[1][0]) - 3.0L) < 1e-9L,
          "matrix getter preserves dimensions and cells");

    expectResult(&runtime, CALC_MODE_MATRIX, "det(MatA)", "-2");
    expectResult(&runtime, CALC_MODE_MATRIX, "determinant(mata)", "-2");
    expectResult(&runtime, CALC_MODE_MATRIX, "transpose(MatA)",
                 "[1,3;2,4]");
    expectResult(&runtime, CALC_MODE_MATRIX, "add(MatA;MatB)",
                 "[5,5;5,5]");
    expectResult(&runtime, CALC_MODE_MATRIX, "sub(MatA;MatB)",
                 "[-3,-1;1,3]");
    expectResult(&runtime, CALC_MODE_MATRIX, "mul(MatA;MatB)",
                 "[8,5;20,13]");
    expectContains(&runtime, CALC_MODE_MATRIX, "inv(MatA)", "-0.5");

    /* Evaluation in another mode must not clear named work areas. */
    expectResult(&runtime, CALC_MODE_COMP, "6*7", "42");
    expectResult(&runtime, CALC_MODE_MATRIX, "det(MatA)", "-2");
    check(modeMatrixSetRegisterExpression(&runtime, MODE_REGISTER_D,
                                          "MatA") == CALC_OK,
          "matrix expression API copies a named register");
    expectResult(&runtime, CALC_MODE_MATRIX, "det(MatD)", "-2");

    direct[0][0] = number(2.0L);
    direct[0][1] = number(0.0L);
    direct[1][0] = number(0.0L);
    direct[1][1] = number(5.0L);
    check(modeMatrixSetRegister(&runtime, MODE_REGISTER_C, direct, 2, 2),
          "numeric matrix setter stores a fixed-size register");
    expectResult(&runtime, CALC_MODE_MATRIX, "det(MatC)", "10");
    check(!modeMatrixSetRegister(&runtime, MODE_REGISTER_C, direct, 5, 2),
          "numeric matrix setter rejects dimensions above 4x4");

    /* Inline operands and historical colon/pipe commands stay compatible. */
    expectResult(&runtime, CALC_MODE_MATRIX, "det([1,2;3,4])", "-2");
    expectResult(&runtime, CALC_MODE_MATRIX,
                 "mul:[1,2;3,4]|[4,3;2,1]", "[8,5;20,13]");
}

static void testMaximumMatrixText(void)
{
    static const char fullIdentity[] =
        "[" MATRIX_LONG_ONE  "," MATRIX_LONG_ZERO ","
            MATRIX_LONG_ZERO "," MATRIX_LONG_ZERO ";"
            MATRIX_LONG_ZERO "," MATRIX_LONG_ONE  ","
            MATRIX_LONG_ZERO "," MATRIX_LONG_ZERO ";"
            MATRIX_LONG_ZERO "," MATRIX_LONG_ZERO ","
            MATRIX_LONG_ONE  "," MATRIX_LONG_ZERO ";"
            MATRIX_LONG_ZERO "," MATRIX_LONG_ZERO ","
            MATRIX_LONG_ZERO "," MATRIX_LONG_ONE  "]";
    static const char expectedDouble[] =
        "[2,0,0,0;0,2,0,0;0,0,2,0;0,0,0,2]";
    static const char overlongRow[] =
        "0000000000000000," MATRIX_LONG_ZERO ","
        MATRIX_LONG_ZERO "," MATRIX_LONG_ZERO;
    ModeRuntime runtime;
    ModeMatrixRegister stored;
    char expression[530];

    modeRuntimeInit(&runtime);
    check(strlen(fullIdentity) == 257,
          "bracketed 4x4 maximum text is exactly 257 characters");
    check(modeMatrixSetRegisterExpression(&runtime, MODE_REGISTER_A,
                                          fullIdentity) == CALC_OK,
          "register expression API accepts all sixteen 15-character cells");
    check(modeMatrixGetRegister(&runtime, MODE_REGISTER_A, &stored) &&
          stored.rows == 4 && stored.columns == 4 &&
          fabsl(calcNumberToLongDouble(stored.cell[3][3]) - 1.0L) < 1e-9L,
          "maximum-size register text preserves a complete 4x4 matrix");

    snprintf(expression, sizeof(expression), "det(%s)", fullIdentity);
    expectResult(&runtime, CALC_MODE_MATRIX, expression, "1");

    snprintf(expression, sizeof(expression), "add(%s;%s)",
             fullIdentity, fullIdentity);
    check(strlen(expression) == 520,
          "binary maximum-size canonical call is exactly 520 characters");
    expectResult(&runtime, CALC_MODE_MATRIX, expression, expectedDouble);

    check(strlen(overlongRow) == 64,
          "row-overflow fixture is exactly 64 characters");
    check(modeMatrixSetRegisterExpression(&runtime, MODE_REGISTER_B,
                                          overlongRow) == CALC_ERR_SYNTAX,
          "matrix row limit remains 63 visible characters");
}

static void testVectorRegisters(void)
{
    ModeRuntime runtime;
    ModeVectorRegister stored;
    CalcNumber direct[MODE_VECTOR_MAX_DIMENSIONS];

    modeRuntimeInit(&runtime);
    check(strcmp(modeVectorRegisterLabel(MODE_REGISTER_A), "VctA") == 0 &&
          strcmp(modeVectorRegisterLabel(MODE_REGISTER_D), "VctD") == 0,
          "vector labels are VctA through VctD");
    check(!modeVectorGetRegister(&runtime, MODE_REGISTER_A, &stored),
          "vector registers start undefined");
    check(modeEvaluate(&runtime, CALC_MODE_VECTOR, "norm(VctA)") ==
              CALC_ERR_DOMAIN,
          "using an undefined vector register is a domain error");
    check(modeVectorSetRegisterExpression(&runtime, MODE_REGISTER_A,
                                          "[3,4]") == CALC_OK,
          "VctA accepts bracketed vector text");
    check(modeVectorSetRegisterExpression(&runtime, MODE_REGISTER_B,
                                          "1,0") == CALC_OK,
          "VctB accepts legacy inline vector text");
    check(modeVectorGetRegister(&runtime, MODE_REGISTER_A, &stored) &&
          stored.dimensions == 2 && stored.defined &&
          fabsl(calcNumberToLongDouble(stored.component[1]) - 4.0L) < 1e-9L,
          "vector getter preserves dimensions and components");
    expectResult(&runtime, CALC_MODE_VECTOR, "norm(VctA)", "5");
    expectResult(&runtime, CALC_MODE_VECTOR, "dot(VctA;VctB)", "3");
    expectResult(&runtime, CALC_MODE_VECTOR, "scale(2;VctA)", "[6,8]");

    check(modeVectorSetRegisterExpression(&runtime, MODE_REGISTER_C,
                                          "[1,0,0]") == CALC_OK &&
          modeVectorSetRegisterExpression(&runtime, MODE_REGISTER_D,
                                          "[0,1,0]") == CALC_OK,
          "three-dimensional vector registers store independently");
    expectResult(&runtime, CALC_MODE_VECTOR, "cross(VctC;VctD)",
                 "[0,0,1]");
    expectContains(&runtime, CALC_MODE_VECTOR, "angle(VctC;VctD)",
                   "1.570");

    direct[0] = number(5.0L);
    direct[1] = number(12.0L);
    direct[2] = number(0.0L);
    check(modeVectorSetRegister(&runtime, MODE_REGISTER_B, direct, 2),
          "numeric vector setter stores a fixed-size register");
    expectResult(&runtime, CALC_MODE_VECTOR, "norm(VctB)", "13");
    check(!modeVectorSetRegister(&runtime, MODE_REGISTER_B, direct, 4),
          "numeric vector setter rejects dimensions above three");

    /* Canonical inline calls and historical aliases remain valid. */
    expectResult(&runtime, CALC_MODE_VECTOR, "norm([3,4])", "5");
    expectResult(&runtime, CALC_MODE_VECTOR, "scale:2;3,4", "[6,8]");
}

int main(void)
{
    testMatrixRegisters();
    testMaximumMatrixText();
    testVectorRegisters();
    if (failures != 0)
        return 1;
    printf("matrix_vector_register_test: PASS (ModeRuntime=%lu bytes)\n",
           (unsigned long)sizeof(ModeRuntime));
    return 0;
}
