#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

typedef struct VectorValue {
    long double value[3];
    u8 count;
} VectorValue;

static u8 vectorRegisterIndex(const char *text, ModeNamedRegister *name)
{
    u8 index;

    for (index = 0; index < MODE_NAMED_REGISTER_COUNT; index++) {
        if (modeEquals(text,
                       modeVectorRegisterLabel((ModeNamedRegister)index))) {
            *name = (ModeNamedRegister)index;
            return 1;
        }
    }
    return 0;
}

static u8 parseVector(ModeRuntime *runtime, const char *text,
                      VectorValue *vector)
{
    char unwrapped[128];
    const char *start = text;
    u16 length;
    u8 error;

    memset(vector, 0, sizeof(*vector));
    while (*start == ' ' || *start == '\t')
        start++;
    length = (u16)strlen(start);
    while (length > 0 && (start[length - 1] == ' ' ||
                          start[length - 1] == '\t'))
        length--;
    if (length == 0 || length >= sizeof(unwrapped))
        return CALC_ERR_SYNTAX;
    memcpy(unwrapped, start, length);
    unwrapped[length] = '\0';
    {
        ModeNamedRegister name;
        if (vectorRegisterIndex(unwrapped, &name)) {
            ModeVectorRegister stored;
            u8 index;

            if (!modeVectorGetRegister(runtime, name, &stored))
                return CALC_ERR_DOMAIN;
            vector->count = stored.dimensions;
            for (index = 0; index < stored.dimensions; index++)
                vector->value[index] = calcNumberToLongDouble(
                    stored.component[index]);
            return CALC_OK;
        }
    }
    if (unwrapped[0] == '[') {
        if (length < 2 || unwrapped[length - 1] != ']')
            return CALC_ERR_SYNTAX;
        memmove(unwrapped, unwrapped + 1, length - 2);
        unwrapped[length - 2] = '\0';
    }
    error = modeParseRealList(runtime, unwrapped, ',', vector->value, 3,
                              &vector->count);
    if (error != CALC_OK)
        return error;
    return (vector->count == 2 || vector->count == 3) ? CALC_OK :
                                                       CALC_ERR_SYNTAX;
}

u8 modeVectorSetRegisterExpression(ModeRuntime *runtime,
                                   ModeNamedRegister name,
                                   const char *expression)
{
    VectorValue parsed;
    CalcNumber components[MODE_VECTOR_MAX_DIMENSIONS];
    u8 index;
    u8 error;

    if (runtime == 0 || expression == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT)
        return CALC_ERR_SYNTAX;
    error = parseVector(runtime, expression, &parsed);
    if (error != CALC_OK)
        return error;
    memset(components, 0, sizeof(components));
    for (index = 0; index < parsed.count; index++) {
        components[index] = calcNumberFromLongDouble(parsed.value[index],
                                                      &error);
        if (error != CALC_OK)
            return error;
    }
    return modeVectorSetRegister(runtime, name, components, parsed.count) ?
           CALC_OK : CALC_ERR_SYNTAX;
}

static long double dot(VectorValue left, VectorValue right)
{
    u8 count = left.count > right.count ? left.count : right.count;
    u8 index;
    long double result = 0.0L;
    for (index = 0; index < count; index++)
        result += left.value[index] * right.value[index];
    return result;
}

u8 modeVectorEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[16];
    char arguments[160];
    const char *body = expression;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));
    char operands[2][64];
    char output[96];
    char a[24];
    char b[24];
    char c[24];
    VectorValue left = {{0.0L, 0.0L, 0.0L}, 0};
    VectorValue right = {{0.0L, 0.0L, 0.0L}, 0};
    u8 count;
    u8 error;

    if (!command) {
        long double norm;
        error = parseVector(runtime, expression, &left);
        if (error != CALC_OK)
            return error;
        norm = sqrtl(dot(left, left));
        modeFormatReal(norm, output, sizeof(output), 9);
        modeSetResult(runtime, output, CALC_OK);
        return CALC_OK;
    }
    body = arguments;
    if (modeEquals(name, "norm")) {
        long double norm;
        error = parseVector(runtime, body, &left);
        if (error != CALC_OK)
            return error;
        norm = sqrtl(dot(left, left));
        modeFormatReal(norm, output, sizeof(output), 9);
    } else {
        if (!modeSplit(body, ';', operands, 2, &count) || count != 2)
            return CALC_ERR_SYNTAX;
        if (modeEquals(name, "scale")) {
            long double scale;
            error = modeEvalReal(runtime, operands[0], &scale);
            if (error != CALC_OK)
                return error;
            error = parseVector(runtime, operands[1], &left);
            if (error != CALC_OK)
                return error;
            left.value[0] *= scale;
            left.value[1] *= scale;
            left.value[2] *= scale;
            modeFormatReal(left.value[0], a, sizeof(a), 7);
            modeFormatReal(left.value[1], b, sizeof(b), 7);
            modeFormatReal(left.value[2], c, sizeof(c), 7);
            if (left.count == 2)
                snprintf(output, sizeof(output), "[%s,%s]", a, b);
            else
                snprintf(output, sizeof(output), "[%s,%s,%s]", a, b, c);
        } else {
            error = parseVector(runtime, operands[0], &left);
            if (error != CALC_OK)
                return error;
            error = parseVector(runtime, operands[1], &right);
            if (error != CALC_OK || left.count != right.count)
                return error == CALC_OK ? CALC_ERR_DOMAIN : error;
            if (modeEquals(name, "dot")) {
                modeFormatReal(dot(left, right), output, sizeof(output), 9);
            } else if (modeEquals(name, "cross")) {
                long double cross[3];
                if (left.count != 3)
                    return CALC_ERR_DOMAIN;
                cross[0] = left.value[1] * right.value[2] -
                           left.value[2] * right.value[1];
                cross[1] = left.value[2] * right.value[0] -
                           left.value[0] * right.value[2];
                cross[2] = left.value[0] * right.value[1] -
                           left.value[1] * right.value[0];
                modeFormatReal(cross[0], a, sizeof(a), 7);
                modeFormatReal(cross[1], b, sizeof(b), 7);
                modeFormatReal(cross[2], c, sizeof(c), 7);
                snprintf(output, sizeof(output), "[%s,%s,%s]", a, b, c);
            } else if (modeEquals(name, "angle")) {
                long double denominator = sqrtl(dot(left, left) * dot(right, right));
                long double cosine;
                long double angle;
                if (denominator == 0.0L)
                    return CALC_ERR_DIVZERO;
                cosine = dot(left, right) / denominator;
                if (cosine > 1.0L)
                    cosine = 1.0L;
                if (cosine < -1.0L)
                    cosine = -1.0L;
                angle = acosl(cosine);
                if (runtime->calc.angleMode == CALC_ANGLE_DEG)
                    angle *= 180.0L / 3.14159265358979323846L;
                else if (runtime->calc.angleMode == CALC_ANGLE_GRAD)
                    angle *= 200.0L / 3.14159265358979323846L;
                modeFormatReal(angle, output, sizeof(output), 9);
            } else
                return CALC_ERR_SYNTAX;
        }
    }
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}
