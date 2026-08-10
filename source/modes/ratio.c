#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

static u8 parseRatioSide(ModeRuntime *runtime, const char *text,
                         long double *left, long double *right)
{
    char fields[2][64];
    u8 count;
    u8 error;

    if (!modeSplit(text, ':', fields, 2, &count) || count != 2)
        return CALC_ERR_SYNTAX;
    error = modeEvalReal(runtime, fields[0], left);
    if (error != CALC_OK)
        return error;
    return modeEvalReal(runtime, fields[1], right);
}

static u8 parseProportion(ModeRuntime *runtime, const char *expression,
                          long double values[4], u8 *missing)
{
    char sides[2][64];
    char fields[2][64];
    u8 sideCount;
    u8 side;
    u8 missingCount = 0;

    if (!modeSplit(expression, '=', sides, 2, &sideCount) ||
        sideCount != 2)
        return CALC_ERR_SYNTAX;
    for (side = 0; side < 2; side++) {
        u8 fieldCount;
        u8 field;
        if (!modeSplit(sides[side], ':', fields, 2, &fieldCount) ||
            fieldCount != 2)
            return CALC_ERR_SYNTAX;
        for (field = 0; field < 2; field++) {
            u8 index = (u8)(side * 2 + field);
            if (modeEquals(fields[field], "?")) {
                *missing = index;
                missingCount++;
            } else {
                u8 error = modeEvalReal(runtime, fields[field],
                                        &values[index]);
                if (error != CALC_OK)
                    return error;
            }
        }
    }
    return missingCount == 1 ? CALC_OK : CALC_ERR_SYNTAX;
}

static u8 solveProportion(const long double values[4], u8 missing,
                          long double *answer)
{
    long double divisor;

    if ((missing != 1 && values[1] == 0.0L) ||
        (missing != 3 && values[3] == 0.0L))
        return CALC_ERR_DIVZERO;

    switch (missing) {
    case 0:                         /* ?:b=c:d */
        divisor = values[3];
        *answer = values[1] * values[2];
        break;
    case 1:                         /* a:?=c:d */
        divisor = values[2];
        *answer = values[0] * values[3];
        break;
    case 2:                         /* a:b=?:d */
        divisor = values[1];
        *answer = values[0] * values[3];
        break;
    case 3:                         /* a:b=c:? */
        divisor = values[0];
        *answer = values[1] * values[2];
        break;
    default:
        return CALC_ERR_SYNTAX;
    }
    if (divisor == 0.0L)
        return CALC_ERR_DIVZERO;
    *answer /= divisor;
    if (!isfinite((double)*answer))
        return CALC_ERR_RANGE;
    /* A ratio denominator may not be zero, including when it is the missing
       term being solved. */
    if ((missing == 1 || missing == 3) && *answer == 0.0L)
        return CALC_ERR_DIVZERO;
    return CALC_OK;
}

static u8 approximateRatio(long double value, s64 *numerator,
                           s64 *denominator)
{
    const s64 limit = 9000000000000000000LL;
    const long double target = fabsl(value);
    const long double tolerance = target * 5e-9L;
    long double remainder = target;
    s64 previousNumerator = 0;
    s64 currentNumerator = 1;
    s64 previousDenominator = 1;
    s64 currentDenominator = 0;
    u8 iteration;

    if (value == 0.0L) {
        *numerator = 0;
        *denominator = 1;
        return 1;
    }
    if (!isfinite((double)value) || target > (long double)limit)
        return 0;
    for (iteration = 0; iteration < 32; iteration++) {
        long double integral = floorl(remainder);
        s64 term;
        s64 nextNumerator;
        s64 nextDenominator;
        long double fraction;

        if (integral > (long double)limit)
            break;
        term = (s64)integral;
        if ((currentNumerator != 0 &&
             term > (limit - previousNumerator) / currentNumerator) ||
            (currentDenominator != 0 &&
             term > (limit - previousDenominator) / currentDenominator))
            break;
        nextNumerator = term * currentNumerator + previousNumerator;
        nextDenominator = term * currentDenominator + previousDenominator;
        previousNumerator = currentNumerator;
        currentNumerator = nextNumerator;
        previousDenominator = currentDenominator;
        currentDenominator = nextDenominator;
        if (currentDenominator != 0 &&
            fabsl((long double)currentNumerator / currentDenominator -
                  target) <= tolerance)
            break;
        fraction = remainder - integral;
        if (fraction == 0.0L)
            break;
        remainder = 1.0L / fraction;
        if (!isfinite((double)remainder))
            break;
    }
    if (currentNumerator == 0 || currentDenominator == 0 ||
        fabsl((long double)currentNumerator / currentDenominator - target) >
            tolerance)
        return 0;
    *numerator = value < 0.0L ? -currentNumerator : currentNumerator;
    *denominator = currentDenominator;
    return 1;
}

u8 modeRatioEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[20];
    char arguments[160];
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));
    const char *equals = strchr(expression, '=');
    char output[64];

    if (command && (modeEquals(name, "ratio1") ||
                    modeEquals(name, "ratio2"))) {
        long double fields[3];
        long double answer;
        u8 count;
        u8 error = modeParseRealList(runtime, arguments, ';', fields, 3,
                                     &count);
        if (error != CALC_OK || count != 3)
            return error == CALC_OK ? CALC_ERR_SYNTAX : error;
        if (modeEquals(name, "ratio1")) {
            if (fields[1] == 0.0L || fields[2] == 0.0L)
                return CALC_ERR_DIVZERO;
            answer = fields[0] * fields[2] / fields[1];
        } else {
            if (fields[0] == 0.0L || fields[1] == 0.0L)
                return CALC_ERR_DIVZERO;
            answer = fields[1] * fields[2] / fields[0];
        }
        if (!isfinite((double)answer))
            return CALC_ERR_RANGE;
        if (modeEquals(name, "ratio2") && answer == 0.0L)
            return CALC_ERR_DIVZERO;
        modeFormatReal(answer, output, sizeof(output), 9);
        modeSetResult(runtime, output, CALC_OK);
        return CALC_OK;
    }
    if (command && modeEquals(name, "ratio")) {
        char fields[2][64];
        u8 count;
        if (!modeSplit(arguments, ';', fields, 2, &count) || count != 2)
            return CALC_ERR_SYNTAX;
        {
            char legacy[132];
            if (snprintf(legacy, sizeof(legacy), "%s:%s", fields[0],
                         fields[1]) >= (int)sizeof(legacy))
                return CALC_ERR_RANGE;
            return modeRatioEvaluate(runtime, legacy);
        }
    }
    if (command)
        return CALC_ERR_SYNTAX;

    if (equals != 0) {
        long double values[4] = {0.0L, 0.0L, 0.0L, 0.0L};
        long double answer;
        u8 missing;
        u8 error = parseProportion(runtime, expression, values, &missing);
        if (error == CALC_OK)
            error = solveProportion(values, missing, &answer);
        if (error != CALC_OK)
            return error;
        modeFormatReal(answer, output, sizeof(output), 9);
        modeSetResult(runtime, output, CALC_OK);
        return CALC_OK;
    }
    {
        long double left;
        long double right;
        s64 leftInteger;
        s64 rightInteger;
        u8 error = parseRatioSide(runtime, expression, &left, &right);

        if (error != CALC_OK)
            return error;
        if (right == 0.0L)
            return CALC_ERR_DIVZERO;
        if (!approximateRatio(left / right, &leftInteger, &rightInteger))
            return CALC_ERR_RANGE;
        snprintf(output, sizeof(output), "%lld:%lld",
                 (long long)leftInteger, (long long)rightInteger);
        modeSetResult(runtime, output, CALC_OK);
    }
    return CALC_OK;
}
