#include <string.h>

#include "mode_internal.h"

static const char *const labels[CALC_MODE_COUNT] = {
    "1: COMP", "2: CMPLX", "3: STAT", "4: BASE-N",
    "5: EQN", "6: MATRIX", "7: TABLE", "8: VECTOR",
    "9: INEQ", "10: RATIO", "11: DIST", "12: GRAPHING"
};

static const char *const statLabels[STAT_MODEL_COUNT] = {
    "1-VAR", "A+BX", "A+BX+CX2", "A+B lnX",
    "A e^(BX)", "A B^X", "A X^B", "A+B/X"
};

static const char *const matrixRegisterLabels[MODE_NAMED_REGISTER_COUNT] = {
    "MatA", "MatB", "MatC", "MatD"
};

static const char *const vectorRegisterLabels[MODE_NAMED_REGISTER_COUNT] = {
    "VctA", "VctB", "VctC", "VctD"
};

void modeRuntimeInit(ModeRuntime *runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    calcContextInit(&runtime->calc);
    runtime->statModel = STAT_MODEL_1VAR;
    runtime->baseRadix = BASE_RADIX_DEC;
}

void modeSetStatModel(ModeRuntime *runtime, StatModel model)
{
    if (runtime != 0 && (u8)model < STAT_MODEL_COUNT)
        runtime->statModel = model;
}

StatModel modeGetStatModel(const ModeRuntime *runtime)
{
    if (runtime == 0 || (u8)runtime->statModel >= STAT_MODEL_COUNT)
        return STAT_MODEL_1VAR;
    return runtime->statModel;
}

const char *modeStatModelLabel(StatModel model)
{
    if ((u8)model >= STAT_MODEL_COUNT)
        return "?";
    return statLabels[model];
}

static u8 validBaseRadix(BaseRadix radix)
{
    return radix == BASE_RADIX_BIN || radix == BASE_RADIX_OCT ||
           radix == BASE_RADIX_DEC || radix == BASE_RADIX_HEX;
}

u8 modeSetBaseRadix(ModeRuntime *runtime, BaseRadix radix)
{
    if (runtime == 0 || !validBaseRadix(radix))
        return 0;
    runtime->baseRadix = radix;
    return 1;
}

BaseRadix modeGetBaseRadix(const ModeRuntime *runtime)
{
    if (runtime == 0 || !validBaseRadix(runtime->baseRadix))
        return BASE_RADIX_DEC;
    return runtime->baseRadix;
}

const char *modeBaseRadixLabel(BaseRadix radix)
{
    switch (radix) {
    case BASE_RADIX_BIN: return "BIN";
    case BASE_RADIX_OCT: return "OCT";
    case BASE_RADIX_DEC: return "DEC";
    case BASE_RADIX_HEX: return "HEX";
    default: return "?";
    }
}

const char *modeMatrixRegisterLabel(ModeNamedRegister name)
{
    if ((u8)name >= MODE_NAMED_REGISTER_COUNT)
        return "?";
    return matrixRegisterLabels[(u8)name];
}

const char *modeVectorRegisterLabel(ModeNamedRegister name)
{
    if ((u8)name >= MODE_NAMED_REGISTER_COUNT)
        return "?";
    return vectorRegisterLabels[(u8)name];
}

u8 modeMatrixSetRegister(ModeRuntime *runtime, ModeNamedRegister name,
                         CalcNumber cells[MODE_MATRIX_MAX_ROWS]
                                         [MODE_MATRIX_MAX_COLUMNS],
                         u8 rows, u8 columns)
{
    ModeMatrixRegister *destination;
    u8 row;
    u8 column;

    if (runtime == 0 || cells == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT || rows == 0 ||
        rows > MODE_MATRIX_MAX_ROWS || columns == 0 ||
        columns > MODE_MATRIX_MAX_COLUMNS)
        return 0;
    destination = &runtime->matrix[(u8)name];
    memset(destination, 0, sizeof(*destination));
    destination->rows = rows;
    destination->columns = columns;
    destination->defined = 1;
    for (row = 0; row < rows; row++)
        for (column = 0; column < columns; column++)
            destination->cell[row][column] = cells[row][column];
    return 1;
}

u8 modeMatrixGetRegister(const ModeRuntime *runtime, ModeNamedRegister name,
                         ModeMatrixRegister *value)
{
    if (value != 0)
        memset(value, 0, sizeof(*value));
    if (runtime == 0 || value == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT ||
        !runtime->matrix[(u8)name].defined)
        return 0;
    *value = runtime->matrix[(u8)name];
    return 1;
}

u8 modeVectorSetRegister(ModeRuntime *runtime, ModeNamedRegister name,
                         const CalcNumber *components, u8 dimensions)
{
    ModeVectorRegister *destination;
    u8 index;

    if (runtime == 0 || components == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT || dimensions < 2 ||
        dimensions > MODE_VECTOR_MAX_DIMENSIONS)
        return 0;
    destination = &runtime->vector[(u8)name];
    memset(destination, 0, sizeof(*destination));
    destination->dimensions = dimensions;
    destination->defined = 1;
    for (index = 0; index < dimensions; index++)
        destination->component[index] = components[index];
    return 1;
}

u8 modeVectorGetRegister(const ModeRuntime *runtime, ModeNamedRegister name,
                         ModeVectorRegister *value)
{
    if (value != 0)
        memset(value, 0, sizeof(*value));
    if (runtime == 0 || value == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT ||
        !runtime->vector[(u8)name].defined)
        return 0;
    *value = runtime->vector[(u8)name];
    return 1;
}

const char *modeLabel(CalcMode mode)
{
    if ((u8)mode >= CALC_MODE_COUNT)
        return "?";
    return labels[mode];
}

u8 modeEvaluate(ModeRuntime *runtime, CalcMode mode, const char *expression)
{
    u8 error;

    if (runtime == 0 || expression == 0 || (u8)mode >= CALC_MODE_COUNT)
        return CALC_ERR_SYNTAX;
    runtime->result[0] = '\0';
    runtime->error = CALC_OK;
    memset(&runtime->table, 0, sizeof(runtime->table));
    switch (mode) {
    case CALC_MODE_COMP:
        error = modeCompEvaluate(runtime, expression);
        break;
    case CALC_MODE_CMPLX:
        error = modeComplexEvaluate(runtime, expression);
        break;
    case CALC_MODE_STAT:
        error = modeStatEvaluate(runtime, expression);
        break;
    case CALC_MODE_BASEN:
        error = modeBaseEvaluate(runtime, expression);
        break;
    case CALC_MODE_EQN:
        error = modeEquationEvaluate(runtime, expression);
        break;
    case CALC_MODE_MATRIX:
        error = modeMatrixEvaluate(runtime, expression);
        break;
    case CALC_MODE_TABLE:
        error = modeTableEvaluate(runtime, expression);
        break;
    case CALC_MODE_VECTOR:
        error = modeVectorEvaluate(runtime, expression);
        break;
    case CALC_MODE_INEQ:
        error = modeInequalityEvaluate(runtime, expression);
        break;
    case CALC_MODE_RATIO:
        error = modeRatioEvaluate(runtime, expression);
        break;
    case CALC_MODE_DIST:
        error = modeDistributionEvaluate(runtime, expression);
        break;
    case CALC_MODE_GRAPHING:
        modeSetResult(runtime, "GRAPH READY", CALC_OK);
        error = CALC_OK;
        break;
    default:
        error = CALC_ERR_SYNTAX;
        break;
    }
    runtime->error = error;
    if (error != CALC_OK && runtime->result[0] == '\0')
        modeSetResult(runtime, calcErrorText(error), error);
    return error;
}
