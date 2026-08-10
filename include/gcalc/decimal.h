#ifndef GCALC_DECIMAL_H
#define GCALC_DECIMAL_H

#include "gcalc/calc.h"

#define DECIMAL_MAX_DIGITS 32

typedef struct DecimalNumber {
    s8 sign;
    s16 exponent;
    u8 count;
    u8 digits[DECIMAL_MAX_DIGITS];
} DecimalNumber;

DecimalNumber decimalZero(void);
DecimalNumber decimalFromS32(s32 value);
DecimalNumber decimalFromCalcNumber(CalcNumber value);
DecimalNumber decimalFromString(const char *text, u8 *error);
DecimalNumber decimalAdd(DecimalNumber left, DecimalNumber right);
DecimalNumber decimalSubtract(DecimalNumber left, DecimalNumber right);
DecimalNumber decimalMultiply(DecimalNumber left, DecimalNumber right);
DecimalNumber decimalDivide(DecimalNumber numerator,
                            DecimalNumber denominator, u8 *error);
void decimalFormat(DecimalNumber value, char *buffer, u8 bufferSize,
                   u8 significantDigits);

#endif
