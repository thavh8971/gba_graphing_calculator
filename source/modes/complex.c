#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "mode_internal.h"

typedef struct ComplexValue {
    long double real;
    long double imaginary;
} ComplexValue;

typedef struct ComplexParser {
    const char *text;
    u16 position;
    u8 error;
} ComplexParser;

static ComplexValue complexMake(long double real, long double imaginary)
{
    ComplexValue value;
    value.real = real;
    value.imaginary = imaginary;
    return value;
}

static void complexSkip(ComplexParser *parser)
{
    while (parser->text[parser->position] == ' ' ||
           parser->text[parser->position] == '\t')
        parser->position++;
}

static ComplexValue complexMultiply(ComplexValue left, ComplexValue right)
{
    return complexMake(left.real * right.real -
                       left.imaginary * right.imaginary,
                       left.real * right.imaginary +
                       left.imaginary * right.real);
}

static ComplexValue complexDivide(ComplexParser *parser,
                                  ComplexValue left, ComplexValue right)
{
    long double denominator = right.real * right.real +
                              right.imaginary * right.imaginary;
    if (denominator == 0.0L) {
        parser->error = CALC_ERR_DIVZERO;
        return complexMake(0.0L, 0.0L);
    }
    return complexMake((left.real * right.real +
                        left.imaginary * right.imaginary) / denominator,
                       (left.imaginary * right.real -
                        left.real * right.imaginary) / denominator);
}

static ComplexValue complexExpression(ComplexParser *parser);

static ComplexValue complexPrimary(ComplexParser *parser)
{
    char number[48];
    u8 length = 0;
    u8 decimal = 0;
    long double value;
    char *end;

    complexSkip(parser);
    if (parser->text[parser->position] == '(') {
        ComplexValue inner;
        parser->position++;
        inner = complexExpression(parser);
        complexSkip(parser);
        if (parser->text[parser->position] != ')')
            parser->error = CALC_ERR_SYNTAX;
        else
            parser->position++;
        return inner;
    }
    if (parser->text[parser->position] == 'i' ||
        parser->text[parser->position] == 'I') {
        parser->position++;
        return complexMake(0.0L, 1.0L);
    }
    while ((size_t)length + 1U < sizeof(number)) {
        char current = parser->text[parser->position];
        if ((current >= '0' && current <= '9') || current == '.') {
            if (current == '.')
                decimal = 1;
            number[length++] = current;
            parser->position++;
        } else if (current == ',' && !decimal) {
            number[length++] = '.';
            parser->position++;
            decimal = 1;
        } else if ((current == 'e' || current == 'E') && length != 0) {
            number[length++] = current;
            parser->position++;
            if ((parser->text[parser->position] == '+' ||
                 parser->text[parser->position] == '-') &&
                (size_t)length + 1U < sizeof(number))
                number[length++] = parser->text[parser->position++];
        } else
            break;
    }
    if (length == 0) {
        parser->error = CALC_ERR_SYNTAX;
        return complexMake(0.0L, 0.0L);
    }
    number[length] = '\0';
    value = strtold(number, &end);
    if (*end != '\0') {
        parser->error = CALC_ERR_SYNTAX;
        return complexMake(0.0L, 0.0L);
    }
    complexSkip(parser);
    if (parser->text[parser->position] == 'i' ||
        parser->text[parser->position] == 'I') {
        parser->position++;
        return complexMake(0.0L, value);
    }
    return complexMake(value, 0.0L);
}

static ComplexValue complexUnary(ComplexParser *parser)
{
    complexSkip(parser);
    if (parser->text[parser->position] == '+') {
        parser->position++;
        return complexUnary(parser);
    }
    if (parser->text[parser->position] == '-') {
        ComplexValue value;
        parser->position++;
        value = complexUnary(parser);
        value.real = -value.real;
        value.imaginary = -value.imaginary;
        return value;
    }
    return complexPrimary(parser);
}

static ComplexValue complexPower(ComplexParser *parser)
{
    ComplexValue base = complexUnary(parser);
    complexSkip(parser);
    if (parser->text[parser->position] == '^') {
        ComplexValue exponent;
        ComplexValue result = complexMake(1.0L, 0.0L);
        s32 integer;
        u8 negative;

        parser->position++;
        exponent = complexUnary(parser);
        if (exponent.imaginary != 0.0L ||
            exponent.real < -64.0L || exponent.real > 64.0L ||
            fabsl(exponent.real - roundl(exponent.real)) > 1e-12L) {
            parser->error = CALC_ERR_POWER;
            return result;
        }
        integer = (s32)llroundl(exponent.real);
        negative = integer < 0;
        if (negative)
            integer = -integer;
        while (integer != 0) {
            if (integer & 1)
                result = complexMultiply(result, base);
            integer >>= 1;
            if (integer != 0)
                base = complexMultiply(base, base);
        }
        if (negative)
            result = complexDivide(parser, complexMake(1.0L, 0.0L), result);
        return result;
    }
    return base;
}

static ComplexValue complexTerm(ComplexParser *parser)
{
    ComplexValue value = complexPower(parser);
    while (!parser->error) {
        char operation;
        ComplexValue right;
        complexSkip(parser);
        operation = parser->text[parser->position];
        if (operation != '*' && operation != '/')
            break;
        parser->position++;
        right = complexPower(parser);
        if (operation == '*')
            value = complexMultiply(value, right);
        else
            value = complexDivide(parser, value, right);
    }
    return value;
}

static ComplexValue complexExpression(ComplexParser *parser)
{
    ComplexValue value = complexTerm(parser);
    while (!parser->error) {
        char operation;
        ComplexValue right;
        complexSkip(parser);
        operation = parser->text[parser->position];
        if (operation != '+' && operation != '-')
            break;
        parser->position++;
        right = complexTerm(parser);
        if (operation == '+') {
            value.real += right.real;
            value.imaginary += right.imaginary;
        } else {
            value.real -= right.real;
            value.imaginary -= right.imaginary;
        }
    }
    return value;
}

static u8 parseComplex(const char *text, ComplexValue *result)
{
    ComplexParser parser;
    parser.text = text;
    parser.position = 0;
    parser.error = CALC_OK;
    *result = complexExpression(&parser);
    complexSkip(&parser);
    if (!parser.error && parser.text[parser.position] != '\0')
        parser.error = CALC_ERR_SYNTAX;
    return parser.error;
}

static void formatComplexValue(ComplexValue value, char *output, u16 capacity)
{
    char real[32];
    char imaginary[32];

    if (fabsl(value.real) < 0.5e-10L)
        value.real = 0.0L;
    if (fabsl(value.imaginary) < 0.5e-10L)
        value.imaginary = 0.0L;
    modeFormatReal(value.real, real, sizeof(real), 9);
    modeFormatReal(fabsl(value.imaginary), imaginary, sizeof(imaginary), 9);
    if (value.imaginary == 0.0L)
        snprintf(output, capacity, "%s", real);
    else if (value.real == 0.0L) {
        if (value.imaginary == 1.0L)
            snprintf(output, capacity, "i");
        else if (value.imaginary == -1.0L)
            snprintf(output, capacity, "-i");
        else
            snprintf(output, capacity, "%s%si",
                     value.imaginary < 0.0L ? "-" : "", imaginary);
    } else
        snprintf(output, capacity, "%s%c%si", real,
                 value.imaginary < 0.0L ? '-' : '+', imaginary);
}

static long double toRadians(const CalcContext *context, long double value)
{
    if (context->angleMode == CALC_ANGLE_DEG)
        return value * 3.14159265358979323846L / 180.0L;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return value * 3.14159265358979323846L / 200.0L;
    return value;
}

static long double fromRadians(const CalcContext *context, long double value)
{
    if (context->angleMode == CALC_ANGLE_DEG)
        return value * 180.0L / 3.14159265358979323846L;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return value * 200.0L / 3.14159265358979323846L;
    return value;
}

u8 modeComplexEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[20];
    char arguments[160];
    const char *body = expression;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));
    ComplexValue value;
    char output[96];
    u8 error;

    if (!command) {
        error = parseComplex(expression, &value);
        if (error != CALC_OK)
            return modeCompEvaluate(runtime, expression);
        formatComplexValue(value, output, sizeof(output));
    } else if (modeEquals(name, "polar") || modeEquals(name, "rect")) {
        char fields[2][64];
        long double parts[2];
        u8 count;
        u8 index;

        body = arguments;
        if (!modeSplit(body, ';', fields, 2, &count) || count != 2)
            return CALC_ERR_SYNTAX;
        for (index = 0; index < 2; index++) {
            error = modeEvalReal(runtime, fields[index], &parts[index]);
            if (error != CALC_OK)
                return error;
        }
        if (modeEquals(name, "polar")) {
            long double angle = toRadians(&runtime->calc, parts[1]);
            value = complexMake(parts[0] * cosl(angle), parts[0] * sinl(angle));
            formatComplexValue(value, output, sizeof(output));
        } else {
            char radius[32];
            char angle[32];
            modeFormatReal(hypotl(parts[0], parts[1]), radius,
                           sizeof(radius), 9);
            modeFormatReal(fromRadians(&runtime->calc,
                                       atan2l(parts[1], parts[0])),
                           angle, sizeof(angle), 9);
            snprintf(output, sizeof(output), "R=%s TH=%s", radius, angle);
        }
    } else if (modeEquals(name, "pow")) {
        char fields[2][64];
        u8 count;
        long double exponent;
        s32 integer;
        ComplexValue result = complexMake(1.0L, 0.0L);
        u8 negative;

        body = arguments;
        if (!modeSplit(body, ';', fields, 2, &count) || count != 2)
            return CALC_ERR_SYNTAX;
        error = parseComplex(fields[0], &value);
        if (error != CALC_OK)
            return error;
        error = modeEvalReal(runtime, fields[1], &exponent);
        if (error != CALC_OK || exponent < -64.0L || exponent > 64.0L ||
            fabsl(exponent - roundl(exponent)) > 1e-12L)
            return error == CALC_OK ? CALC_ERR_POWER : error;
        integer = (s32)llroundl(exponent);
        negative = integer < 0;
        if (negative)
            integer = -integer;
        while (integer != 0) {
            if (integer & 1)
                result = complexMultiply(result, value);
            integer >>= 1;
            if (integer != 0)
                value = complexMultiply(value, value);
        }
        if (negative) {
            ComplexParser parser = {"", 0, CALC_OK};
            result = complexDivide(&parser, complexMake(1.0L, 0.0L), result);
            if (parser.error)
                return parser.error;
        }
        formatComplexValue(result, output, sizeof(output));
    } else if (modeEquals(name, "conj") || modeEquals(name, "re") ||
               modeEquals(name, "im") || modeEquals(name, "abs") ||
               modeEquals(name, "norm") || modeEquals(name, "arg")) {
        body = arguments;
        error = parseComplex(body, &value);
        if (error != CALC_OK)
            return error;
        if (modeEquals(name, "conj")) {
            value.imaginary = -value.imaginary;
            formatComplexValue(value, output, sizeof(output));
        } else if (modeEquals(name, "re"))
            modeFormatReal(value.real, output, sizeof(output), 9);
        else if (modeEquals(name, "im"))
            modeFormatReal(value.imaginary, output, sizeof(output), 9);
        else if (modeEquals(name, "abs") || modeEquals(name, "norm"))
            modeFormatReal(hypotl(value.real, value.imaginary), output,
                           sizeof(output), 9);
        else if (modeEquals(name, "arg"))
            modeFormatReal(fromRadians(&runtime->calc,
                                       atan2l(value.imaginary, value.real)),
                           output, sizeof(output), 9);
        else
            return CALC_ERR_SYNTAX;
    } else
        return modeCompEvaluate(runtime, expression);
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}
