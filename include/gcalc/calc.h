#ifndef GCALC_CALC_H
#define GCALC_CALC_H

#include "gcalc/types.h"

#define CALC_ONE 256L
#define CALC_SIGNIFICANT_DIGITS 9
#define CALC_MIN_EXPONENT (-100)
#define CALC_MAX_EXPONENT 100

#define CALC_OK 0
#define CALC_ERR_SYNTAX 1
#define CALC_ERR_DIVZERO 2
#define CALC_ERR_DOMAIN 3
#define CALC_ERR_POWER 4
#define CALC_ERR_RANGE 5

#define CALC_ANGLE_RAD 0
#define CALC_ANGLE_DEG 1
#define CALC_ANGLE_GRAD 2

typedef struct CalcNumber {
    s32 mantissa;
    s16 exponent;
} CalcNumber;

typedef struct CalcContext {
    CalcNumber variables[26];
    u32 variableMask;
    CalcNumber answer;
    CalcNumber previousAnswer;
    u32 randomState;
    u8 hasAnswer;
    u8 hasPreviousAnswer;
    u8 angleMode;
} CalcContext;

void calcContextInit(CalcContext *context);
u8 calcContextSetVariable(CalcContext *context, char name, CalcNumber value);
u8 calcContextGetVariable(const CalcContext *context, char name,
                          CalcNumber *value);
void calcEvaluateContext(CalcContext *context, const char *expression,
                         CalcNumber *result, u8 *error);
void calcEvalNumberContext(CalcContext *context, const char *expression,
                           CalcNumber xValue, CalcNumber *result, u8 *error);

/* Compatibility facade used by the original prototype's public contract. */
void calcClearVariables(void);
void calcSetAngleMode(u8 mode);
u8 calcGetAngleMode(void);
u8 calcSetVariable(char name, CalcNumber value);
u8 calcGetVariable(char name, CalcNumber *value);
void calcEvaluate(char *expression, CalcNumber *result, u8 *error);
void calcEvalNumber(char *expression, CalcNumber xValue,
                    CalcNumber *result, u8 *error);

CalcNumber calcNumberFromFixed(s32 fixedValue);
CalcNumber calcNumberFromLongDouble(long double value, u8 *error);
long double calcNumberToLongDouble(CalcNumber value);
CalcNumber calcNumberAdd(CalcNumber left, CalcNumber right);
CalcNumber calcNumberMultiply(CalcNumber left, CalcNumber right);
u8 calcNumberToFixed(CalcNumber value, s32 *fixedValue);
u8 calcTangentFixed(s32 phaseFixed, s32 *fixedValue);
u8 calcTangentPhaseCrosses(CalcNumber phase0, CalcNumber phase1);
void calcFormatNumber(CalcNumber value, char *buffer, u8 bufferSize);
s32 calcEval(char *expression, s32 xValue, u8 *error);
const char *calcErrorText(u8 error);

#endif
