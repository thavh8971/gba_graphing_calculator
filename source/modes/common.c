#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

static char lowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (char)(value - 'A' + 'a');
    return value;
}

u8 modeStartsWith(const char *text, const char *prefix)
{
    while (*prefix != '\0') {
        if (*text == '\0')
            return 0;
        if (lowerAscii(*text++) != lowerAscii(*prefix++))
            return 0;
    }
    return 1;
}

u8 modeEquals(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (lowerAscii(*left++) != lowerAscii(*right++))
            return 0;
    }
    return *left == '\0' && *right == '\0';
}

void modeSetResult(ModeRuntime *runtime, const char *text, u8 error)
{
    u16 index = 0;

    while (text[index] != '\0' && index + 1 < MODE_RESULT_CAPACITY) {
        runtime->result[index] = text[index];
        index++;
    }
    runtime->result[index] = '\0';
    runtime->error = error;
}

void modeFormatReal(long double value, char *buffer, u16 capacity,
                    u8 digits)
{
    char temporary[64];
    char *point;
    char *end;
    char *exponent;

    if (capacity == 0)
        return;
    if (!isfinite((double)value)) {
        snprintf(buffer, capacity, "Math ERROR");
        return;
    }
    if (value == 0.0L)
        value = 0.0L;
    if (digits == 0)
        digits = 1;
    if (digits > 15)
        digits = 15;
    /* MinGW's default UCRT printf ABI treats long double as double even
       though GCC uses a distinct 80-bit long-double type.  Passing an actual
       long double to %Lf consequently prints garbage on that host.  The
       calculator keeps at most nine significant decimal digits, so narrowing
       at this formatting boundary is lossless for values produced by the
       numeric core and is portable across newlib, UCRT and glibc. */
    snprintf(temporary, sizeof(temporary), "%.*g", (int)digits,
             (double)value);
    point = strchr(temporary, '.');
    if (point != 0) {
        exponent = strchr(temporary, 'e');
        if (exponent == 0)
            exponent = strchr(temporary, 'E');
        end = exponent != 0 ? exponent - 1 :
                              temporary + strlen(temporary) - 1;
        while (end > point && *end == '0')
            end--;
        if (end == point)
            end--;
        if (exponent != 0)
            memmove(end + 1, exponent, strlen(exponent) + 1);
        else
            end[1] = '\0';
    }
    snprintf(buffer, capacity, "%s", temporary);
}

u8 modeEvalReal(ModeRuntime *runtime, const char *expression,
                long double *value)
{
    CalcNumber result;
    u8 error;

    calcEvaluateContext(&runtime->calc, expression, &result, &error);
    if (error != CALC_OK)
        return error;
    *value = calcNumberToLongDouble(result);
    return CALC_OK;
}

u8 modeSplit(const char *source, char separator, char output[][64],
             u8 maximum, u8 *count)
{
    u16 start = 0;
    u16 index;
    s16 depth = 0;

    *count = 0;
    for (index = 0;; index++) {
        char value = source[index];
        if (value == '(' || value == '[')
            depth++;
        else if ((value == ')' || value == ']') && depth > 0)
            depth--;
        if ((value == separator && depth == 0) || value == '\0') {
            u16 length = (u16)(index - start);
            u16 copy;
            if (*count >= maximum || length >= 64)
                return 0;
            while (length > 0 && (source[start] == ' ' || source[start] == '\t')) {
                start++;
                length--;
            }
            while (length > 0 && (source[start + length - 1] == ' ' ||
                                  source[start + length - 1] == '\t'))
                length--;
            if (length == 0)
                return 0;
            for (copy = 0; copy < length; copy++)
                output[*count][copy] = source[start + copy];
            output[*count][length] = '\0';
            (*count)++;
            start = (u16)(index + 1);
        }
        if (value == '\0')
            break;
    }
    return *count != 0;
}

u8 modeParseRealList(ModeRuntime *runtime, const char *text,
                     char separator, long double *values,
                     u8 maximum, u8 *count)
{
    char fields[MODE_MAX_VALUES][64];
    u8 index;

    if (maximum > MODE_MAX_VALUES)
        maximum = MODE_MAX_VALUES;
    if (!modeSplit(text, separator, fields, maximum, count))
        return CALC_ERR_SYNTAX;
    for (index = 0; index < *count; index++) {
        u8 error = modeEvalReal(runtime, fields[index], &values[index]);
        if (error != CALC_OK)
            return error;
    }
    return CALC_OK;
}

static u8 commandNameCharacter(char value, u8 first)
{
    if ((value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z') || value == '_')
        return 1;
    return !first && ((value >= '0' && value <= '9') || value == '#');
}

/* Extract either canonical name(arguments) syntax or the historical
   name:arguments alias.  The canonical form must consume the whole input;
   nested parentheses/brackets are preserved in the copied argument body. */
u8 modeParseCommand(const char *source, char *name, u8 nameCapacity,
                    char *arguments, u16 argumentCapacity)
{
    const char *start = source;
    const char *cursor;
    const char *bodyStart;
    const char *bodyEnd;
    u16 nameLength;
    u16 bodyLength;
    s16 depth = 0;

    if (source == 0 || name == 0 || arguments == 0 ||
        nameCapacity < 2 || argumentCapacity == 0)
        return 0;
    while (*start == ' ' || *start == '\t')
        start++;
    if (!commandNameCharacter(*start, 1))
        return 0;
    cursor = start + 1;
    while (commandNameCharacter(*cursor, 0))
        cursor++;
    nameLength = (u16)(cursor - start);
    if (nameLength + 1 > nameCapacity)
        return 0;
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    if (*cursor == ':') {
        bodyStart = cursor + 1;
        while (*bodyStart == ' ' || *bodyStart == '\t')
            bodyStart++;
        bodyEnd = bodyStart + strlen(bodyStart);
        while (bodyEnd > bodyStart &&
               (bodyEnd[-1] == ' ' || bodyEnd[-1] == '\t'))
            bodyEnd--;
    } else if (*cursor == '(') {
        const char *scan = cursor;
        bodyStart = cursor + 1;
        for (;;) {
            char value = *scan;
            if (value == '\0')
                return 0;
            if (value == '(' || value == '[')
                depth++;
            else if (value == ')' || value == ']') {
                depth--;
                if (depth < 0)
                    return 0;
                if (depth == 0) {
                    bodyEnd = scan;
                    scan++;
                    while (*scan == ' ' || *scan == '\t')
                        scan++;
                    if (*scan != '\0')
                        return 0;
                    break;
                }
            }
            scan++;
        }
    } else
        return 0;
    bodyLength = (u16)(bodyEnd - bodyStart);
    if (bodyLength + 1 > argumentCapacity)
        return 0;
    memcpy(name, start, nameLength);
    name[nameLength] = '\0';
    memcpy(arguments, bodyStart, bodyLength);
    arguments[bodyLength] = '\0';
    return 1;
}

s64 modeIntegerGcd(s64 left, s64 right)
{
    if (left < 0)
        left = -left;
    if (right < 0)
        right = -right;
    while (right != 0) {
        s64 remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

long double modeCombination(s32 n, s32 k)
{
    long double value = 1.0L;
    s32 index;

    if (n < 0 || k < 0 || k > n)
        return 0.0L;
    if (k > n - k)
        k = n - k;
    for (index = 1; index <= k; index++)
        value = value * (n - k + index) / index;
    return value;
}
