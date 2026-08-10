#ifndef GCALC_MODE_INTERNAL_H
#define GCALC_MODE_INTERNAL_H

#include "gcalc/modes.h"

#define MODE_MAX_VALUES 64

u8 modeCompEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeComplexEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeStatEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeBaseEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeEquationEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeMatrixEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeTableEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeVectorEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeInequalityEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeRatioEvaluate(ModeRuntime *runtime, const char *expression);
u8 modeDistributionEvaluate(ModeRuntime *runtime, const char *expression);

u8 modeStartsWith(const char *text, const char *prefix);
u8 modeEquals(const char *left, const char *right);
void modeSetResult(ModeRuntime *runtime, const char *text, u8 error);
void modeFormatReal(long double value, char *buffer, u16 capacity,
                    u8 digits);
u8 modeEvalReal(ModeRuntime *runtime, const char *expression,
                long double *value);
u8 modeSplit(const char *source, char separator, char output[][64],
             u8 maximum, u8 *count);
u8 modeParseRealList(ModeRuntime *runtime, const char *text,
                     char separator, long double *values,
                     u8 maximum, u8 *count);
u8 modeParseCommand(const char *source, char *name, u8 nameCapacity,
                    char *arguments, u16 argumentCapacity);
s64 modeIntegerGcd(s64 left, s64 right);
long double modeCombination(s32 n, s32 k);

#endif
