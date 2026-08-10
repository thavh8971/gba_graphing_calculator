#include "gcalc/decimal.h"

#include <limits.h>
#include <string.h>

#define DECIMAL_WORK_DIGITS (DECIMAL_MAX_DIGITS * 3)
#define DECIMAL_PRODUCT_DIGITS (DECIMAL_MAX_DIGITS * 2 + 1)

static DecimalNumber decimalMakeZero(void)
{
    DecimalNumber value;

    memset(&value, 0, sizeof(value));
    return value;
}

static void decimalSetError(u8 *error, u8 value)
{
    if (error != NULL)
        *error = value;
}

static u8 decimalIsSpace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' ||
           value == '\n';
}

/*
 * DecimalNumber stores a normalized base-10 significand.  digits[0] has
 * the power in exponent, digits[1] has exponent - 1, and so on.
 */
static DecimalNumber decimalPack(const u8 *digits, u8 count, s8 sign,
                                 s32 exponent)
{
    DecimalNumber value;
    u8 first;
    u8 keep;
    u8 index;
    u8 carried;

    value = decimalMakeZero();
    if (digits == NULL || count == 0 || sign == 0)
        return value;

    first = 0;
    while (first < count && digits[first] == 0)
    {
        first++;
        exponent--;
    }
    if (first == count)
        return value;

    digits += first;
    count = (u8)(count - first);
    keep = count > DECIMAL_MAX_DIGITS ? DECIMAL_MAX_DIGITS : count;

    value.sign = sign < 0 ? -1 : 1;
    value.count = keep;
    for (index = 0; index < keep; index++)
        value.digits[index] = digits[index];

    if (count > keep && digits[keep] >= 5)
    {
        carried = 1;
        index = keep;
        while (index != 0 && carried)
        {
            index--;
            if (value.digits[index] == 9)
                value.digits[index] = 0;
            else
            {
                value.digits[index]++;
                carried = 0;
            }
        }
        if (carried)
        {
            memset(value.digits, 0, sizeof(value.digits));
            value.digits[0] = 1;
            value.count = 1;
            exponent++;
        }
    }

    while (value.count > 1 && value.digits[value.count - 1] == 0)
        value.count--;

    if (exponent > INT16_MAX)
        exponent = INT16_MAX;
    else if (exponent < INT16_MIN)
        exponent = INT16_MIN;
    value.exponent = (s16)exponent;
    return value;
}

DecimalNumber decimalZero(void)
{
    return decimalMakeZero();
}

DecimalNumber decimalFromS32(s32 input)
{
    u8 reversed[10];
    u8 digits[10];
    u8 count;
    u8 index;
    u32 magnitude;
    s8 sign;

    if (input == 0)
        return decimalMakeZero();

    sign = input < 0 ? -1 : 1;
    magnitude = input < 0 ? (u32)(-(input + 1)) + 1U : (u32)input;
    count = 0;
    while (magnitude != 0)
    {
        reversed[count++] = (u8)(magnitude % 10U);
        magnitude /= 10U;
    }
    for (index = 0; index < count; index++)
        digits[index] = reversed[count - index - 1];
    return decimalPack(digits, count, sign, (s32)count - 1);
}

DecimalNumber decimalFromCalcNumber(CalcNumber input)
{
    u8 reversed[10];
    u8 digits[10];
    u8 count;
    u8 index;
    u32 magnitude;
    s8 sign;
    s32 exponent;

    if (input.mantissa == 0)
        return decimalMakeZero();

    sign = input.mantissa < 0 ? -1 : 1;
    magnitude = input.mantissa < 0 ?
                (u32)(-(input.mantissa + 1)) + 1U :
                (u32)input.mantissa;
    count = 0;
    while (magnitude != 0)
    {
        reversed[count++] = (u8)(magnitude % 10U);
        magnitude /= 10U;
    }
    for (index = 0; index < count; index++)
        digits[index] = reversed[count - index - 1];

    exponent = (s32)input.exponent + count - CALC_SIGNIFICANT_DIGITS;
    return decimalPack(digits, count, sign, exponent);
}

DecimalNumber decimalFromString(const char *text, u8 *error)
{
    u8 digits[DECIMAL_WORK_DIGITS];
    u8 count;
    u8 first;
    u8 sawDigit;
    u8 sawPoint;
    s8 sign;
    s8 exponentSign;
    s32 digitsBeforePoint;
    s32 explicitExponent;
    s32 exponent;
    const char *cursor;

    decimalSetError(error, CALC_OK);
    if (text == NULL)
    {
        decimalSetError(error, CALC_ERR_SYNTAX);
        return decimalMakeZero();
    }

    cursor = text;
    while (decimalIsSpace(*cursor))
        cursor++;

    sign = 1;
    if (*cursor == '+' || *cursor == '-')
    {
        if (*cursor == '-')
            sign = -1;
        cursor++;
    }

    count = 0;
    sawDigit = 0;
    sawPoint = 0;
    digitsBeforePoint = 0;
    while ((*cursor >= '0' && *cursor <= '9') ||
           *cursor == '.' || *cursor == ',')
    {
        if (*cursor == '.' || *cursor == ',')
        {
            if (sawPoint)
            {
                decimalSetError(error, CALC_ERR_SYNTAX);
                return decimalMakeZero();
            }
            sawPoint = 1;
        }
        else
        {
            if (count == DECIMAL_WORK_DIGITS)
            {
                decimalSetError(error, CALC_ERR_RANGE);
                return decimalMakeZero();
            }
            digits[count++] = (u8)(*cursor - '0');
            sawDigit = 1;
            if (!sawPoint)
                digitsBeforePoint++;
        }
        cursor++;
    }

    if (!sawDigit)
    {
        decimalSetError(error, CALC_ERR_SYNTAX);
        return decimalMakeZero();
    }

    explicitExponent = 0;
    if (*cursor == 'e' || *cursor == 'E')
    {
        cursor++;
        exponentSign = 1;
        if (*cursor == '+' || *cursor == '-')
        {
            if (*cursor == '-')
                exponentSign = -1;
            cursor++;
        }
        if (*cursor < '0' || *cursor > '9')
        {
            decimalSetError(error, CALC_ERR_SYNTAX);
            return decimalMakeZero();
        }
        while (*cursor >= '0' && *cursor <= '9')
        {
            if (explicitExponent > 100000)
            {
                decimalSetError(error, CALC_ERR_RANGE);
                return decimalMakeZero();
            }
            explicitExponent = explicitExponent * 10 + (*cursor - '0');
            cursor++;
        }
        explicitExponent *= exponentSign;
    }

    while (decimalIsSpace(*cursor))
        cursor++;
    if (*cursor != '\0')
    {
        decimalSetError(error, CALC_ERR_SYNTAX);
        return decimalMakeZero();
    }

    first = 0;
    while (first < count && digits[first] == 0)
        first++;
    if (first == count)
        return decimalMakeZero();

    exponent = digitsBeforePoint + explicitExponent - first - 1;
    if (exponent < INT16_MIN || exponent > INT16_MAX - 1)
    {
        decimalSetError(error, CALC_ERR_RANGE);
        return decimalMakeZero();
    }
    return decimalPack(digits + first, (u8)(count - first), sign, exponent);
}

static s32 decimalLowPower(DecimalNumber value)
{
    return (s32)value.exponent - value.count + 1;
}

static void decimalAlign(DecimalNumber value, s32 highPower, s32 lowPower,
                         s16 *destination, u8 destinationCount)
{
    u8 index;

    for (index = 0; index < value.count; index++)
    {
        s32 power;
        s32 target;

        power = (s32)value.exponent - index;
        target = highPower - power;
        if (power >= lowPower && target >= 0 &&
            target < destinationCount)
            destination[target] = value.digits[index];
    }
}

static s8 decimalAlignedCompare(const s16 *left, const s16 *right,
                                u8 count)
{
    u8 index;

    for (index = 0; index < count; index++)
    {
        if (left[index] < right[index])
            return -1;
        if (left[index] > right[index])
            return 1;
    }
    return 0;
}

DecimalNumber decimalAdd(DecimalNumber left, DecimalNumber right)
{
    s16 leftDigits[DECIMAL_WORK_DIGITS];
    s16 rightDigits[DECIMAL_WORK_DIGITS];
    s16 resultDigits[DECIMAL_WORK_DIGITS];
    u8 packedDigits[DECIMAL_WORK_DIGITS];
    s32 highPower;
    s32 lowPower;
    s32 span;
    u8 count;
    u8 index;
    s16 carry;
    s8 comparison;
    s8 resultSign;

    if (left.sign == 0)
        return right;
    if (right.sign == 0)
        return left;

    highPower = left.exponent > right.exponent ?
                left.exponent : right.exponent;
    lowPower = decimalLowPower(left) < decimalLowPower(right) ?
               decimalLowPower(left) : decimalLowPower(right);
    span = highPower - lowPower;
    if (span > DECIMAL_WORK_DIGITS - 2)
        lowPower = highPower - (DECIMAL_WORK_DIGITS - 2);
    count = (u8)(highPower - lowPower + 2);

    memset(leftDigits, 0, sizeof(leftDigits));
    memset(rightDigits, 0, sizeof(rightDigits));
    memset(resultDigits, 0, sizeof(resultDigits));
    decimalAlign(left, highPower + 1, lowPower, leftDigits, count);
    decimalAlign(right, highPower + 1, lowPower, rightDigits, count);

    if (left.sign == right.sign)
    {
        carry = 0;
        index = count;
        while (index != 0)
        {
            s16 sum;

            index--;
            sum = (s16)(leftDigits[index] + rightDigits[index] + carry);
            resultDigits[index] = (s16)(sum % 10);
            carry = (s16)(sum / 10);
        }
        resultSign = left.sign;
    }
    else
    {
        comparison = decimalAlignedCompare(leftDigits, rightDigits, count);
        if (comparison == 0)
            return decimalMakeZero();
        resultSign = comparison > 0 ? left.sign : right.sign;
        index = count;
        while (index != 0)
        {
            s16 difference;

            index--;
            difference = comparison > 0 ?
                         (s16)(leftDigits[index] - rightDigits[index]) :
                         (s16)(rightDigits[index] - leftDigits[index]);
            resultDigits[index] = difference;
        }
        index = (u8)(count - 1);
        while (index != 0)
        {
            if (resultDigits[index] < 0)
            {
                resultDigits[index] += 10;
                resultDigits[index - 1]--;
            }
            index--;
        }
    }

    for (index = 0; index < count; index++)
        packedDigits[index] = (u8)resultDigits[index];
    return decimalPack(packedDigits, count, resultSign, highPower + 1);
}

DecimalNumber decimalSubtract(DecimalNumber left, DecimalNumber right)
{
    if (right.sign != 0)
        right.sign = (s8)-right.sign;
    return decimalAdd(left, right);
}

DecimalNumber decimalMultiply(DecimalNumber left, DecimalNumber right)
{
    u16 work[DECIMAL_PRODUCT_DIGITS];
    u8 digits[DECIMAL_PRODUCT_DIGITS];
    u8 count;
    u8 leftIndex;
    u8 rightIndex;
    u8 index;
    s32 exponent;

    if (left.sign == 0 || right.sign == 0)
        return decimalMakeZero();

    memset(work, 0, sizeof(work));
    count = (u8)(left.count + right.count);
    for (leftIndex = 0; leftIndex < left.count; leftIndex++)
    {
        for (rightIndex = 0; rightIndex < right.count; rightIndex++)
        {
            work[leftIndex + rightIndex + 1] =
                (u16)(work[leftIndex + rightIndex + 1] +
                      left.digits[leftIndex] * right.digits[rightIndex]);
        }
    }

    index = count;
    while (index > 1)
    {
        index--;
        work[index - 1] = (u16)(work[index - 1] + work[index] / 10);
        work[index] %= 10;
    }
    for (index = 0; index < count; index++)
        digits[index] = (u8)work[index];

    exponent = (s32)left.exponent + right.exponent + 1;
    return decimalPack(digits, count,
                       left.sign == right.sign ? 1 : -1, exponent);
}

static s8 decimalIntegerCompare(const u8 *left, u8 leftCount,
                                const u8 *right, u8 rightCount)
{
    u8 index;

    while (leftCount != 0 && *left == 0)
    {
        left++;
        leftCount--;
    }
    while (rightCount != 0 && *right == 0)
    {
        right++;
        rightCount--;
    }
    if (leftCount < rightCount)
        return -1;
    if (leftCount > rightCount)
        return 1;
    for (index = 0; index < leftCount; index++)
    {
        if (left[index] < right[index])
            return -1;
        if (left[index] > right[index])
            return 1;
    }
    return 0;
}

static void decimalIntegerTrim(u8 *digits, u8 *count)
{
    u8 first;

    first = 0;
    while (first < *count && digits[first] == 0)
        first++;
    if (first == *count)
    {
        *count = 0;
        return;
    }
    if (first != 0)
    {
        memmove(digits, digits + first, (size_t)(*count - first));
        *count = (u8)(*count - first);
    }
}

static void decimalIntegerSubtract(u8 *left, u8 *leftCount,
                                   const u8 *right, u8 rightCount)
{
    s16 leftIndex;
    s16 rightIndex;
    s16 borrow;

    leftIndex = (s16)(*leftCount - 1);
    rightIndex = (s16)(rightCount - 1);
    borrow = 0;
    while (leftIndex >= 0)
    {
        s16 digit;

        digit = (s16)(left[leftIndex] - borrow);
        if (rightIndex >= 0)
            digit = (s16)(digit - right[rightIndex]);
        if (digit < 0)
        {
            digit += 10;
            borrow = 1;
        }
        else
            borrow = 0;
        left[leftIndex] = (u8)digit;
        leftIndex--;
        rightIndex--;
    }
    decimalIntegerTrim(left, leftCount);
}

static void decimalIntegerTimesTen(u8 *digits, u8 *count)
{
    if (*count != 0 && *count < DECIMAL_MAX_DIGITS + 1)
        digits[(*count)++] = 0;
}

DecimalNumber decimalDivide(DecimalNumber numerator,
                            DecimalNumber denominator, u8 *error)
{
    u8 dividend[DECIMAL_MAX_DIGITS];
    u8 divisor[DECIMAL_MAX_DIGITS];
    u8 remainder[DECIMAL_MAX_DIGITS + 1];
    u8 quotient[DECIMAL_MAX_DIGITS + 1];
    u8 width;
    u8 index;
    u8 remainderCount;
    u8 quotientDigit;
    s8 comparison;
    s32 exponent;

    decimalSetError(error, CALC_OK);
    if (denominator.sign == 0)
    {
        decimalSetError(error, CALC_ERR_DIVZERO);
        return decimalMakeZero();
    }
    if (numerator.sign == 0)
        return decimalMakeZero();

    width = numerator.count > denominator.count ?
            numerator.count : denominator.count;
    memset(dividend, 0, sizeof(dividend));
    memset(divisor, 0, sizeof(divisor));
    for (index = 0; index < numerator.count; index++)
        dividend[index] = numerator.digits[index];
    for (index = 0; index < denominator.count; index++)
        divisor[index] = denominator.digits[index];

    comparison = decimalIntegerCompare(dividend, width, divisor, width);
    exponent = (s32)numerator.exponent - denominator.exponent;
    if (comparison < 0)
        exponent--;
    if (exponent < INT16_MIN || exponent > INT16_MAX - 1)
    {
        decimalSetError(error, CALC_ERR_RANGE);
        return decimalMakeZero();
    }

    memcpy(remainder, dividend, width);
    remainderCount = width;
    decimalIntegerTrim(remainder, &remainderCount);
    if (comparison < 0)
        decimalIntegerTimesTen(remainder, &remainderCount);

    for (index = 0; index < DECIMAL_MAX_DIGITS + 1; index++)
    {
        quotientDigit = 0;
        while (decimalIntegerCompare(remainder, remainderCount,
                                     divisor, width) >= 0)
        {
            decimalIntegerSubtract(remainder, &remainderCount,
                                   divisor, width);
            quotientDigit++;
        }
        quotient[index] = quotientDigit;
        if (index + 1 < DECIMAL_MAX_DIGITS + 1)
            decimalIntegerTimesTen(remainder, &remainderCount);
    }

    return decimalPack(quotient, DECIMAL_MAX_DIGITS + 1,
                       numerator.sign == denominator.sign ? 1 : -1,
                       exponent);
}

static void decimalAppend(char *buffer, u8 bufferSize, u8 *position,
                          char value)
{
    if (bufferSize == 0)
        return;
    if (*position < (u8)(bufferSize - 1))
    {
        buffer[*position] = value;
        (*position)++;
    }
    buffer[*position] = '\0';
}

static void decimalAppendText(char *buffer, u8 bufferSize, u8 *position,
                              const char *text)
{
    while (*text != '\0')
    {
        decimalAppend(buffer, bufferSize, position, *text);
        text++;
    }
}

static void decimalAppendExponent(char *buffer, u8 bufferSize,
                                  u8 *position, s16 exponent)
{
    char reversed[6];
    u8 count;
    u16 magnitude;

    if (exponent < 0)
    {
        decimalAppend(buffer, bufferSize, position, '-');
        magnitude = (u16)(-(exponent + 1)) + 1U;
    }
    else
        magnitude = (u16)exponent;

    count = 0;
    do
    {
        reversed[count++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0);

    while (count != 0)
        decimalAppend(buffer, bufferSize, position, reversed[--count]);
}

static DecimalNumber decimalRoundForFormat(DecimalNumber value,
                                           u8 significantDigits)
{
    u8 index;
    u8 carried;

    if (value.sign == 0 || significantDigits == 0 ||
        value.count <= significantDigits)
        return value;

    carried = value.digits[significantDigits] >= 5;
    value.count = significantDigits;
    index = value.count;
    while (index != 0 && carried)
    {
        index--;
        if (value.digits[index] == 9)
            value.digits[index] = 0;
        else
        {
            value.digits[index]++;
            carried = 0;
        }
    }
    if (carried)
    {
        memset(value.digits, 0, sizeof(value.digits));
        value.digits[0] = 1;
        value.count = 1;
        if (value.exponent < INT16_MAX)
            value.exponent++;
    }
    while (value.count > 1 && value.digits[value.count - 1] == 0)
        value.count--;
    return value;
}

void decimalFormat(DecimalNumber value, char *buffer, u8 bufferSize,
                   u8 significantDigits)
{
    u8 position;
    u8 index;
    u8 limit;
    u8 scientificThreshold;
    s16 point;

    if (buffer == NULL || bufferSize == 0)
        return;
    buffer[0] = '\0';
    position = 0;
    value = decimalRoundForFormat(value, significantDigits);

    if (value.sign == 0 || value.count == 0)
    {
        decimalAppend(buffer, bufferSize, &position, '0');
        return;
    }
    if (value.sign < 0)
        decimalAppend(buffer, bufferSize, &position, '-');

    limit = value.count;
    scientificThreshold = significantDigits == 0 ? 15 : significantDigits;
    if (value.exponent >= scientificThreshold || value.exponent <= -6)
    {
        decimalAppend(buffer, bufferSize, &position,
                      (char)('0' + value.digits[0]));
        if (limit > 1)
        {
            decimalAppend(buffer, bufferSize, &position, '.');
            for (index = 1; index < limit; index++)
                decimalAppend(buffer, bufferSize, &position,
                              (char)('0' + value.digits[index]));
        }
        decimalAppend(buffer, bufferSize, &position, 'E');
        decimalAppendExponent(buffer, bufferSize, &position, value.exponent);
        return;
    }

    point = (s16)(value.exponent + 1);
    if (point <= 0)
    {
        decimalAppendText(buffer, bufferSize, &position, "0.");
        while (point < 0)
        {
            decimalAppend(buffer, bufferSize, &position, '0');
            point++;
        }
        for (index = 0; index < limit; index++)
            decimalAppend(buffer, bufferSize, &position,
                          (char)('0' + value.digits[index]));
    }
    else
    {
        for (index = 0; index < limit; index++)
        {
            if (index == (u8)point)
                decimalAppend(buffer, bufferSize, &position, '.');
            decimalAppend(buffer, bufferSize, &position,
                          (char)('0' + value.digits[index]));
        }
        while (point > limit)
        {
            decimalAppend(buffer, bufferSize, &position, '0');
            point--;
        }
    }

    while (position != 0 && buffer[position - 1] == '0' &&
           strchr(buffer, '.') != NULL)
        buffer[--position] = '\0';
    if (position != 0 && buffer[position - 1] == '.')
        buffer[--position] = '\0';
}
