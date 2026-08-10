#ifndef GCALC_MODES_H
#define GCALC_MODES_H

#include "gcalc/calc.h"

#define MODE_RESULT_CAPACITY 160
#define TABLE_MAX_ROWS 16
#define MODE_NAMED_REGISTER_COUNT 4
#define MODE_MATRIX_MAX_ROWS 4
#define MODE_MATRIX_MAX_COLUMNS 4
#define MODE_VECTOR_MAX_DIMENSIONS 3

typedef enum CalcMode {
    CALC_MODE_COMP,
    CALC_MODE_CMPLX,
    CALC_MODE_STAT,
    CALC_MODE_BASEN,
    CALC_MODE_EQN,
    CALC_MODE_MATRIX,
    CALC_MODE_TABLE,
    CALC_MODE_VECTOR,
    CALC_MODE_INEQ,
    CALC_MODE_RATIO,
    CALC_MODE_DIST,
    CALC_MODE_GRAPHING,
    CALC_MODE_COUNT
} CalcMode;

/* STAT follows the eight calculation types exposed by the dedicated
   statistics selector.  The selected model is runtime state so a table UI
   can submit just its serialized rows; an explicit model call may override
   it for scripting and tests. */
typedef enum StatModel {
    STAT_MODEL_1VAR,
    STAT_MODEL_LINEAR,
    STAT_MODEL_QUADRATIC,
    STAT_MODEL_LOGARITHMIC,
    STAT_MODEL_EXPONENTIAL,
    STAT_MODEL_AB_EXPONENTIAL,
    STAT_MODEL_POWER,
    STAT_MODEL_INVERSE,
    STAT_MODEL_COUNT
} StatModel;

/* Values intentionally equal their mathematical radices. */
typedef enum BaseRadix {
    BASE_RADIX_BIN = 2,
    BASE_RADIX_OCT = 8,
    BASE_RADIX_DEC = 10,
    BASE_RADIX_HEX = 16
} BaseRadix;

/* MATRIX and VECTOR each expose four independent, persistent work areas.
   The enum suffix maps directly to the calculator labels MatA..MatD and
   VctA..VctD.  Values use CalcNumber so storage has the same precision and
   binary layout on the host and ARM targets. */
typedef enum ModeNamedRegister {
    MODE_REGISTER_A,
    MODE_REGISTER_B,
    MODE_REGISTER_C,
    MODE_REGISTER_D,
    MODE_REGISTER_COUNT = MODE_NAMED_REGISTER_COUNT
} ModeNamedRegister;

typedef struct ModeMatrixRegister {
    CalcNumber cell[MODE_MATRIX_MAX_ROWS][MODE_MATRIX_MAX_COLUMNS];
    u8 rows;
    u8 columns;
    u8 defined;
} ModeMatrixRegister;

typedef struct ModeVectorRegister {
    CalcNumber component[MODE_VECTOR_MAX_DIMENSIONS];
    u8 dimensions;
    u8 defined;
} ModeVectorRegister;

typedef struct TableResult {
    char x[TABLE_MAX_ROWS][16];
    char y[TABLE_MAX_ROWS][16];
    char z[TABLE_MAX_ROWS][16];
    char derivative[TABLE_MAX_ROWS][16];
    u8 rows;
    u8 functionCount;
    u8 hasDerivative;
} TableResult;

typedef struct ModeRuntime {
    CalcContext calc;
    TableResult table;
    ModeMatrixRegister matrix[MODE_NAMED_REGISTER_COUNT];
    ModeVectorRegister vector[MODE_NAMED_REGISTER_COUNT];
    char result[MODE_RESULT_CAPACITY];
    u8 error;
    StatModel statModel;
    BaseRadix baseRadix;
} ModeRuntime;

void modeRuntimeInit(ModeRuntime *runtime);
u8 modeEvaluate(ModeRuntime *runtime, CalcMode mode, const char *expression);
const char *modeLabel(CalcMode mode);

void modeSetStatModel(ModeRuntime *runtime, StatModel model);
StatModel modeGetStatModel(const ModeRuntime *runtime);
const char *modeStatModelLabel(StatModel model);
u8 modeStatEvaluateRows(ModeRuntime *runtime, StatModel model,
                        const char *const xValues[],
                        const char *const yValues[],
                        const char *const frequencies[], u8 count);

u8 modeSetBaseRadix(ModeRuntime *runtime, BaseRadix radix);
BaseRadix modeGetBaseRadix(const ModeRuntime *runtime);
const char *modeBaseRadixLabel(BaseRadix radix);
u8 modeBaseDigitValid(BaseRadix radix, char digit);
u8 modeBaseFormatValue(BaseRadix radix, s32 value, char *output,
                       u16 capacity);

const char *modeMatrixRegisterLabel(ModeNamedRegister name);
const char *modeVectorRegisterLabel(ModeNamedRegister name);
u8 modeMatrixSetRegister(ModeRuntime *runtime, ModeNamedRegister name,
                         CalcNumber cells[MODE_MATRIX_MAX_ROWS]
                                         [MODE_MATRIX_MAX_COLUMNS],
                         u8 rows, u8 columns);
u8 modeMatrixGetRegister(const ModeRuntime *runtime, ModeNamedRegister name,
                         ModeMatrixRegister *value);
u8 modeMatrixSetRegisterExpression(ModeRuntime *runtime,
                                   ModeNamedRegister name,
                                   const char *expression);
u8 modeVectorSetRegister(ModeRuntime *runtime, ModeNamedRegister name,
                         const CalcNumber *components, u8 dimensions);
u8 modeVectorGetRegister(const ModeRuntime *runtime, ModeNamedRegister name,
                         ModeVectorRegister *value);
u8 modeVectorSetRegisterExpression(ModeRuntime *runtime,
                                   ModeNamedRegister name,
                                   const char *expression);

#endif
