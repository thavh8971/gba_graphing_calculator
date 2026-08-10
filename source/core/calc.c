#include "gcalc/calc.h"
#include "gcalc/syntax.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#define CALC_PI 3.141592653589793238462643383279502884L
#define CALC_E  2.718281828459045235360287471352662498L
#define CALC_EVAL_MAX_DEPTH 112
#define CALC_ITERATION_LIMIT 100000LL
#define CALC_PARSE_CACHE_SLOTS 4
#define CALC_PARSE_CACHE_TEXT 128

typedef struct EvalState {
    CalcContext *context;
    const CalcSyntaxAst *ast;
    long double bindings[26];
    u32 bindingMask;
    u8 error;
    u8 depth;
} EvalState;

static CalcContext defaultContext;
static u8 defaultContextReady;

/*
 * Parsing dominates graph sampling when the same formula is evaluated at
 * hundreds of adjacent X/T values.  Expressions in the public editor are
 * capped at 127 bytes, so a small immutable round-robin cache removes that
 * repeated work without embedding a multi-kilobyte AST in every graph row.
 * Four entries cover rectangular, parametric and tangent-phase evaluation in
 * the same frame while consuming only EWRAM/BSS, not the ARM7TDMI stack.
 */
typedef struct CalcParseCacheEntry {
    CalcSyntaxAst ast;
    char source[CALC_PARSE_CACHE_TEXT];
    u8 valid;
} CalcParseCacheEntry;

static CalcParseCacheEntry parseCache[CALC_PARSE_CACHE_SLOTS] GCALC_EWRAM_BSS;
static u8 parseCacheNext;
static CalcSyntaxAst oversizeAst GCALC_EWRAM_BSS;

static const CalcSyntaxAst *cachedSyntaxAst(const char *expression)
{
    CalcParseCacheEntry *entry;
    size_t length;
    u8 index;

    if (expression == NULL)
        return NULL;
    length = strlen(expression);
    if (length >= CALC_PARSE_CACHE_TEXT) {
        if (calcSyntaxParse(expression, &oversizeAst) != CALC_OK)
            return NULL;
        return &oversizeAst;
    }
    for (index = 0; index < CALC_PARSE_CACHE_SLOTS; ++index) {
        entry = &parseCache[index];
        if (entry->valid && strcmp(entry->source, expression) == 0)
            return &entry->ast;
    }
    entry = &parseCache[parseCacheNext];
    parseCacheNext = (u8)((parseCacheNext + 1) % CALC_PARSE_CACHE_SLOTS);
    entry->valid = 0;
    if (calcSyntaxParse(expression, &entry->ast) != CALC_OK)
        return NULL;
    memcpy(entry->source, expression, length + 1);
    entry->valid = 1;
    return &entry->ast;
}

static char lowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z')
        return (char)(value + ('a' - 'A'));
    return value;
}

static u8 equalText(const char *left, const char *right)
{
    while (*left && *right)
    {
        if (lowerAscii(*left) != lowerAscii(*right))
            return 0;
        ++left;
        ++right;
    }
    return (u8)(*left == '\0' && *right == '\0');
}

static long double absoluteValue(long double value)
{
    return value < 0.0L ? -value : value;
}

static void setError(EvalState *state, u8 error)
{
    if (state->error == CALC_OK)
        state->error = error;
}

static u8 finiteValue(long double value)
{
    return (u8)isfinite(value);
}

static long double angleToRadians(const CalcContext *context,
                                  long double value)
{
    if (context->angleMode == CALC_ANGLE_DEG)
        return value * CALC_PI / 180.0L;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return value * CALC_PI / 200.0L;
    return value;
}

static long double radiansToAngle(const CalcContext *context,
                                  long double value)
{
    if (context->angleMode == CALC_ANGLE_DEG)
        return value * 180.0L / CALC_PI;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return value * 200.0L / CALC_PI;
    return value;
}

static void ensureDefaultContext(void)
{
    if (!defaultContextReady)
    {
        calcContextInit(&defaultContext);
        defaultContextReady = 1;
    }
}

void calcContextInit(CalcContext *context)
{
    if (context == NULL)
        return;
    memset(context, 0, sizeof(*context));
    context->randomState = 0x6d2b79f5UL;
    context->angleMode = CALC_ANGLE_RAD;
}

u8 calcContextSetVariable(CalcContext *context, char name, CalcNumber value)
{
    u8 index;

    if (context == NULL)
        return 0;
    name = lowerAscii(name);
    if (name < 'a' || name > 'z')
        return 0;
    index = (u8)(name - 'a');
    context->variables[index] = value;
    context->variableMask |= (u32)1U << index;
    return 1;
}

u8 calcContextGetVariable(const CalcContext *context, char name,
                          CalcNumber *value)
{
    u8 index;

    if (context == NULL || value == NULL)
        return 0;
    name = lowerAscii(name);
    if (name < 'a' || name > 'z')
        return 0;
    index = (u8)(name - 'a');
    if ((context->variableMask & ((u32)1U << index)) == 0)
        return 0;
    *value = context->variables[index];
    return 1;
}

CalcNumber calcNumberFromLongDouble(long double value, u8 *error)
{
    CalcNumber result;
    long double magnitude;
    long double scaled;
    long double rounded;
    s16 exponent;
    s32 mantissa;

    result.mantissa = 0;
    result.exponent = 0;
    if (error != NULL)
        *error = CALC_OK;
    if (!finiteValue(value))
    {
        if (error != NULL)
            *error = CALC_ERR_RANGE;
        return result;
    }
    if (value == 0.0L)
        return result;

    magnitude = absoluteValue(value);
    exponent = (s16)floorl(log10l(magnitude));
    scaled = magnitude / powl(10.0L, (long double)exponent);
    while (scaled >= 10.0L)
    {
        scaled /= 10.0L;
        ++exponent;
    }
    while (scaled < 1.0L)
    {
        scaled *= 10.0L;
        --exponent;
    }
    if (exponent < CALC_MIN_EXPONENT || exponent > CALC_MAX_EXPONENT)
    {
        if (error != NULL)
            *error = CALC_ERR_RANGE;
        return result;
    }
    rounded = floorl(scaled * 100000000.0L + 0.5L);
    if (rounded >= 1000000000.0L)
    {
        rounded = 100000000.0L;
        ++exponent;
        if (exponent > CALC_MAX_EXPONENT)
        {
            if (error != NULL)
                *error = CALC_ERR_RANGE;
            return result;
        }
    }
    mantissa = (s32)rounded;
    result.mantissa = value < 0.0L ? -mantissa : mantissa;
    result.exponent = exponent;
    return result;
}

long double calcNumberToLongDouble(CalcNumber value)
{
    if (value.mantissa == 0)
        return 0.0L;
    return ((long double)value.mantissa / 100000000.0L) *
           powl(10.0L, (long double)value.exponent);
}

CalcNumber calcNumberFromFixed(s32 fixedValue)
{
    u8 error;
    return calcNumberFromLongDouble((long double)fixedValue /
                                    (long double)CALC_ONE, &error);
}

CalcNumber calcNumberAdd(CalcNumber left, CalcNumber right)
{
    u8 error;
    return calcNumberFromLongDouble(calcNumberToLongDouble(left) +
                                    calcNumberToLongDouble(right), &error);
}

CalcNumber calcNumberMultiply(CalcNumber left, CalcNumber right)
{
    u8 error;
    return calcNumberFromLongDouble(calcNumberToLongDouble(left) *
                                    calcNumberToLongDouble(right), &error);
}

u8 calcNumberToFixed(CalcNumber value, s32 *fixedValue)
{
    long double scaled;
    long double rounded;

    if (fixedValue == NULL)
        return CALC_ERR_RANGE;
    scaled = calcNumberToLongDouble(value) * (long double)CALC_ONE;
    if (!finiteValue(scaled) || scaled > (long double)INT32_MAX ||
        scaled < (long double)INT32_MIN)
        return CALC_ERR_RANGE;
    rounded = scaled < 0.0L ? ceill(scaled - 0.5L) : floorl(scaled + 0.5L);
    *fixedValue = (s32)rounded;
    return CALC_OK;
}

static long double parseNumberText(const char *text, u8 *error)
{
    long double value = 0.0L;
    long double fraction = 0.1L;
    s16 exponent = 0;
    u8 exponentNegative = 0;
    u8 decimal = 0;
    u8 digits = 0;

    while (*text && *text != 'e' && *text != 'E')
    {
        if (*text >= '0' && *text <= '9')
        {
            digits = 1;
            if (decimal)
            {
                value += (long double)(*text - '0') * fraction;
                fraction *= 0.1L;
            }
            else
                value = value * 10.0L + (long double)(*text - '0');
        }
        else if ((*text == '.' || *text == ',') && !decimal)
            decimal = 1;
        else
        {
            *error = CALC_ERR_SYNTAX;
            return 0.0L;
        }
        ++text;
    }
    if (!digits)
    {
        *error = CALC_ERR_SYNTAX;
        return 0.0L;
    }
    if (*text == 'e' || *text == 'E')
    {
        ++text;
        if (*text == '+' || *text == '-')
        {
            exponentNegative = (u8)(*text == '-');
            ++text;
        }
        if (*text < '0' || *text > '9')
        {
            *error = CALC_ERR_SYNTAX;
            return 0.0L;
        }
        while (*text >= '0' && *text <= '9')
        {
            if (exponent < 1000)
                exponent = (s16)(exponent * 10 + (*text - '0'));
            ++text;
        }
        if (*text != '\0')
        {
            *error = CALC_ERR_SYNTAX;
            return 0.0L;
        }
        if (exponentNegative)
            exponent = (s16)-exponent;
        value *= powl(10.0L, (long double)exponent);
    }
    if (!finiteValue(value))
        *error = CALC_ERR_RANGE;
    return value;
}

static u32 nextRandom(CalcContext *context)
{
    context->randomState = context->randomState * 1664525UL + 1013904223UL;
    return context->randomState;
}

static long double randomUnit(CalcContext *context)
{
    return (long double)(nextRandom(context) >> 8) / 16777216.0L;
}

static u8 integerValue(long double value, s64 *integer)
{
    long double rounded;

    if (!finiteValue(value) || value > 9007199254740991.0L ||
        value < -9007199254740991.0L)
        return 0;
    rounded = value < 0.0L ? ceill(value - 0.5L) : floorl(value + 0.5L);
    if (absoluteValue(value - rounded) > 1.0e-10L)
        return 0;
    *integer = (s64)rounded;
    return 1;
}

static s64 greatestCommonDivisor(s64 left, s64 right)
{
    s64 remainder;

    if (left < 0)
        left = -left;
    if (right < 0)
        right = -right;
    while (right != 0)
    {
        remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static long double combination(s64 n, s64 r, u8 *valid)
{
    long double result = 1.0L;
    s64 index;

    if (n < 0 || r < 0 || r > n)
    {
        *valid = 0;
        return 0.0L;
    }
    if (r > n - r)
        r = n - r;
    for (index = 1; index <= r; ++index)
        result = result * (long double)(n - r + index) / (long double)index;
    *valid = (u8)finiteValue(result);
    return result;
}

static long double normalCdfStandard(long double value)
{
    return 0.5L * erfcl(-value / sqrtl(2.0L));
}

static long double normalInverseStandard(long double probability)
{
    long double low = -12.0L;
    long double high = 12.0L;
    u8 iteration;

    for (iteration = 0; iteration < 96; ++iteration)
    {
        long double middle = (low + high) * 0.5L;
        if (normalCdfStandard(middle) < probability)
            low = middle;
        else
            high = middle;
    }
    return (low + high) * 0.5L;
}

static long double evaluateNode(EvalState *state, s16 nodeIndex);

static u8 variableNode(const EvalState *state, s16 nodeIndex, u8 *index)
{
    const CalcSyntaxNode *node;
    char name;

    if (nodeIndex < 0 || nodeIndex >= state->ast->count)
        return 0;
    node = &state->ast->nodes[nodeIndex];
    if (node->kind != CALC_SYNTAX_IDENTIFIER || node->text[0] == '\0' ||
        node->text[1] != '\0')
        return 0;
    name = lowerAscii(node->text[0]);
    if (name < 'a' || name > 'z')
        return 0;
    *index = (u8)(name - 'a');
    return 1;
}

static long double evaluateArgument(EvalState *state,
                                    const CalcSyntaxNode *call, u8 index)
{
    if (index >= call->argCount)
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    return evaluateNode(state, call->args[index]);
}

static void bindVariable(EvalState *state, u8 index, long double value,
                         u8 *wasBound, long double *oldValue)
{
    u32 bit = (u32)1U << index;
    *wasBound = (u8)((state->bindingMask & bit) != 0);
    *oldValue = state->bindings[index];
    state->bindings[index] = value;
    state->bindingMask |= bit;
}

static void restoreVariable(EvalState *state, u8 index, u8 wasBound,
                            long double oldValue)
{
    u32 bit = (u32)1U << index;
    if (wasBound)
        state->bindings[index] = oldValue;
    else
        state->bindingMask &= ~bit;
}

static long double evaluateSeries(EvalState *state,
                                  const CalcSyntaxNode *call, u8 product)
{
    long double lowerValue;
    long double upperValue;
    long double total = product ? 1.0L : 0.0L;
    long double compensation = 0.0L;
    long double oldValue;
    s64 lower;
    s64 upper;
    s64 position;
    s64 count;
    u8 variable = (u8)('x' - 'a');
    u8 wasBound;
    u8 lowerArg;
    u8 upperArg;

    if (call->argCount == 4)
    {
        if (!variableNode(state, call->args[1], &variable))
        {
            setError(state, CALC_ERR_SYNTAX);
            return 0.0L;
        }
        lowerArg = 2;
        upperArg = 3;
    }
    else if (call->argCount == 3)
    {
        lowerArg = 1;
        upperArg = 2;
    }
    else
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    lowerValue = evaluateArgument(state, call, lowerArg);
    upperValue = evaluateArgument(state, call, upperArg);
    if (state->error || !integerValue(lowerValue, &lower) ||
        !integerValue(upperValue, &upper))
    {
        setError(state, CALC_ERR_DOMAIN);
        return 0.0L;
    }
    if (upper < lower)
        return total;
    count = upper - lower + 1;
    if (count > CALC_ITERATION_LIMIT)
    {
        setError(state, CALC_ERR_RANGE);
        return 0.0L;
    }
    bindVariable(state, variable, (long double)lower, &wasBound, &oldValue);
    for (position = lower; position <= upper; ++position)
    {
        long double term;
        state->bindings[variable] = (long double)position;
        term = evaluateNode(state, call->args[0]);
        if (state->error)
            break;
        if (product)
            total *= term;
        else
        {
            long double adjusted = term - compensation;
            long double next = total + adjusted;
            compensation = (next - total) - adjusted;
            total = next;
        }
        if (!finiteValue(total))
        {
            setError(state, CALC_ERR_RANGE);
            break;
        }
        if (position == INT64_MAX)
            break;
    }
    restoreVariable(state, variable, wasBound, oldValue);
    return state->error ? 0.0L : total;
}

static long double evaluateIntegral(EvalState *state,
                                    const CalcSyntaxNode *call)
{
    const u16 intervals = 256;
    long double lower;
    long double upper;
    long double step;
    long double total = 0.0L;
    long double oldValue;
    u16 index;
    u8 variable = (u8)('x' - 'a');
    u8 lowerArg;
    u8 upperArg;
    u8 wasBound;

    if (call->argCount == 4)
    {
        if (!variableNode(state, call->args[1], &variable))
        {
            setError(state, CALC_ERR_SYNTAX);
            return 0.0L;
        }
        lowerArg = 2;
        upperArg = 3;
    }
    else if (call->argCount == 3)
    {
        lowerArg = 1;
        upperArg = 2;
    }
    else
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    lower = evaluateArgument(state, call, lowerArg);
    upper = evaluateArgument(state, call, upperArg);
    if (state->error || !finiteValue(lower) || !finiteValue(upper))
    {
        setError(state, CALC_ERR_DOMAIN);
        return 0.0L;
    }
    if (lower == upper)
        return 0.0L;
    step = (upper - lower) / (long double)intervals;
    bindVariable(state, variable, lower, &wasBound, &oldValue);
    for (index = 0; index <= intervals; ++index)
    {
        long double value;
        u8 weight;
        state->bindings[variable] = lower + step * (long double)index;
        value = evaluateNode(state, call->args[0]);
        if (state->error)
            break;
        weight = (u8)((index == 0 || index == intervals) ? 1 :
                      ((index & 1U) ? 4 : 2));
        total += (long double)weight * value;
    }
    restoreVariable(state, variable, wasBound, oldValue);
    if (state->error)
        return 0.0L;
    total *= step / 3.0L;
    if (!finiteValue(total))
        setError(state, CALC_ERR_RANGE);
    return state->error ? 0.0L : total;
}

static long double evaluateDerivative(EvalState *state,
                                      const CalcSyntaxNode *call,
                                      u8 second)
{
    long double point;
    long double step;
    long double oldValue;
    long double below;
    long double above;
    long double center = 0.0L;
    u8 variable = (u8)('x' - 'a');
    u8 pointArg;
    u8 wasBound;

    if (call->argCount == 3)
    {
        if (!variableNode(state, call->args[1], &variable))
        {
            setError(state, CALC_ERR_SYNTAX);
            return 0.0L;
        }
        pointArg = 2;
    }
    else if (call->argCount == 2)
        pointArg = 1;
    else
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    point = evaluateArgument(state, call, pointArg);
    if (state->error || !finiteValue(point))
    {
        setError(state, CALC_ERR_DOMAIN);
        return 0.0L;
    }
    step = (absoluteValue(point) + 1.0L) * (second ? 1.0e-4L : 1.0e-5L);
    bindVariable(state, variable, point, &wasBound, &oldValue);
    state->bindings[variable] = point - step;
    below = evaluateNode(state, call->args[0]);
    state->bindings[variable] = point + step;
    above = evaluateNode(state, call->args[0]);
    if (second)
    {
        state->bindings[variable] = point;
        center = evaluateNode(state, call->args[0]);
    }
    restoreVariable(state, variable, wasBound, oldValue);
    if (state->error)
        return 0.0L;
    if (second)
        return (above - 2.0L * center + below) / (step * step);
    return (above - below) / (2.0L * step);
}

static long double evaluateFunction(EvalState *state,
                                    const CalcSyntaxNode *call)
{
    long double values[CALC_SYNTAX_MAX_ARGS];
    const char *name = call->text;
    long double result = 0.0L;
    s64 firstInteger;
    s64 secondInteger;
    u8 count;

    if (equalText(name, "sum"))
        return evaluateSeries(state, call, 0);
    if (equalText(name, "prod") || equalText(name, "product"))
        return evaluateSeries(state, call, 1);
    if (equalText(name, "integral") || equalText(name, "integrate"))
        return evaluateIntegral(state, call);
    if (equalText(name, "d/dx") || equalText(name, "derivative") ||
        equalText(name, "diff"))
        return evaluateDerivative(state, call, 0);
    if (equalText(name, "d2/dx2") || equalText(name, "derivative2"))
        return evaluateDerivative(state, call, 1);

    for (count = 0; count < call->argCount; ++count)
    {
        values[count] = evaluateArgument(state, call, count);
        if (state->error)
            return 0.0L;
    }

#define REQUIRE_ARGS(number)                                                   \
    do                                                                         \
    {                                                                          \
        if (call->argCount != (number))                                        \
        {                                                                      \
            setError(state, CALC_ERR_SYNTAX);                                  \
            return 0.0L;                                                       \
        }                                                                      \
    } while (0)

    if (equalText(name, "sin"))
    {
        REQUIRE_ARGS(1);
        result = sinl(angleToRadians(state->context, values[0]));
    }
    else if (equalText(name, "cos"))
    {
        REQUIRE_ARGS(1);
        result = cosl(angleToRadians(state->context, values[0]));
    }
    else if (equalText(name, "tan"))
    {
        long double radians;
        long double cosine;
        REQUIRE_ARGS(1);
        radians = angleToRadians(state->context, values[0]);
        cosine = cosl(radians);
        if (absoluteValue(cosine) < 1.0e-12L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = tanl(radians);
    }
    else if (equalText(name, "asin") || equalText(name, "sin^-1"))
    {
        REQUIRE_ARGS(1);
        if (values[0] < -1.0L || values[0] > 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = radiansToAngle(state->context, asinl(values[0]));
    }
    else if (equalText(name, "acos") || equalText(name, "cos^-1"))
    {
        REQUIRE_ARGS(1);
        if (values[0] < -1.0L || values[0] > 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = radiansToAngle(state->context, acosl(values[0]));
    }
    else if (equalText(name, "atan") || equalText(name, "tan^-1"))
    {
        REQUIRE_ARGS(1);
        result = radiansToAngle(state->context, atanl(values[0]));
    }
    else if (equalText(name, "sinh"))
    {
        REQUIRE_ARGS(1);
        result = sinhl(values[0]);
    }
    else if (equalText(name, "cosh"))
    {
        REQUIRE_ARGS(1);
        result = coshl(values[0]);
    }
    else if (equalText(name, "tanh"))
    {
        REQUIRE_ARGS(1);
        result = tanhl(values[0]);
    }
    else if (equalText(name, "asinh"))
    {
        REQUIRE_ARGS(1);
        result = asinhl(values[0]);
    }
    else if (equalText(name, "acosh"))
    {
        REQUIRE_ARGS(1);
        if (values[0] < 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = acoshl(values[0]);
    }
    else if (equalText(name, "atanh"))
    {
        REQUIRE_ARGS(1);
        if (values[0] <= -1.0L || values[0] >= 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = atanhl(values[0]);
    }
    else if (equalText(name, "sqrt"))
    {
        REQUIRE_ARGS(1);
        if (values[0] < 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = sqrtl(values[0]);
    }
    else if (equalText(name, "cbrt"))
    {
        REQUIRE_ARGS(1);
        result = cbrtl(values[0]);
    }
    else if (equalText(name, "ln"))
    {
        REQUIRE_ARGS(1);
        if (values[0] <= 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = logl(values[0]);
    }
    else if (equalText(name, "log") || equalText(name, "log10") ||
             equalText(name, "lg"))
    {
        REQUIRE_ARGS(1);
        if (values[0] <= 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = log10l(values[0]);
    }
    else if (equalText(name, "logab"))
    {
        REQUIRE_ARGS(2);
        if (values[0] <= 0.0L || values[1] <= 0.0L || values[1] == 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = logl(values[0]) / logl(values[1]);
    }
    else if (equalText(name, "exp"))
    {
        REQUIRE_ARGS(1);
        result = expl(values[0]);
    }
    else if (equalText(name, "pow10"))
    {
        REQUIRE_ARGS(1);
        result = powl(10.0L, values[0]);
    }
    else if (equalText(name, "pow"))
    {
        REQUIRE_ARGS(2);
        if (values[0] == 0.0L && values[1] < 0.0L)
            setError(state, CALC_ERR_DIVZERO);
        else if (values[0] < 0.0L && !integerValue(values[1], &firstInteger))
            setError(state, CALC_ERR_POWER);
        else
            result = powl(values[0], values[1]);
    }
    else if (equalText(name, "root") || equalText(name, "nroot"))
    {
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) || firstInteger == 0)
            setError(state, CALC_ERR_DOMAIN);
        else if (values[1] < 0.0L && (firstInteger & 1LL) == 0)
            setError(state, CALC_ERR_DOMAIN);
        else if (values[1] == 0.0L && firstInteger < 0)
            setError(state, CALC_ERR_DIVZERO);
        else
        {
            result = powl(absoluteValue(values[1]), 1.0L / values[0]);
            if (values[1] < 0.0L)
                result = -result;
        }
    }
    else if (equalText(name, "fac"))
    {
        s64 index;
        REQUIRE_ARGS(1);
        if (!integerValue(values[0], &firstInteger) || firstInteger < 0 ||
            firstInteger > 100000)
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            result = 1.0L;
            for (index = 2; index <= firstInteger; ++index)
                result *= (long double)index;
        }
    }
    else if (equalText(name, "npr"))
    {
        s64 index;
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) ||
            !integerValue(values[1], &secondInteger) || firstInteger < 0 ||
            secondInteger < 0 || secondInteger > firstInteger)
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            result = 1.0L;
            for (index = 0; index < secondInteger; ++index)
                result *= (long double)(firstInteger - index);
        }
    }
    else if (equalText(name, "ncr"))
    {
        u8 valid;
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) ||
            !integerValue(values[1], &secondInteger))
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            result = combination(firstInteger, secondInteger, &valid);
            if (!valid)
                setError(state, CALC_ERR_DOMAIN);
        }
    }
    else if (equalText(name, "gcd") || equalText(name, "lcm"))
    {
        s64 gcd;
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) ||
            !integerValue(values[1], &secondInteger))
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            gcd = greatestCommonDivisor(firstInteger, secondInteger);
            if (equalText(name, "gcd"))
                result = (long double)gcd;
            else if (gcd == 0)
                result = 0.0L;
            else
                result = absoluteValue((long double)(firstInteger / gcd) *
                                       (long double)secondInteger);
        }
    }
    else if (equalText(name, "mod") || equalText(name, "rmdr") ||
             equalText(name, "intdiv"))
    {
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) ||
            !integerValue(values[1], &secondInteger))
            setError(state, CALC_ERR_DOMAIN);
        else if (secondInteger == 0)
            setError(state, CALC_ERR_DIVZERO);
        else if (equalText(name, "intdiv"))
            result = (long double)(firstInteger / secondInteger);
        else
            result = (long double)(firstInteger % secondInteger);
    }
    else if (equalText(name, "min") || equalText(name, "max"))
    {
        REQUIRE_ARGS(2);
        if (equalText(name, "min"))
            result = values[0] < values[1] ? values[0] : values[1];
        else
            result = values[0] > values[1] ? values[0] : values[1];
    }
    else if (equalText(name, "hypot"))
    {
        REQUIRE_ARGS(2);
        result = hypotl(values[0], values[1]);
    }
    else if (equalText(name, "abs") || equalText(name, "norm"))
    {
        REQUIRE_ARGS(1);
        result = absoluteValue(values[0]);
    }
    else if (equalText(name, "recip"))
    {
        REQUIRE_ARGS(1);
        if (values[0] == 0.0L)
            setError(state, CALC_ERR_DIVZERO);
        else
            result = 1.0L / values[0];
    }
    else if (equalText(name, "sqr"))
    {
        REQUIRE_ARGS(1);
        result = values[0] * values[0];
    }
    else if (equalText(name, "sign"))
    {
        REQUIRE_ARGS(1);
        result = values[0] < 0.0L ? -1.0L : (values[0] > 0.0L ? 1.0L : 0.0L);
    }
    else if (equalText(name, "floor") || equalText(name, "intg"))
    {
        REQUIRE_ARGS(1);
        result = floorl(values[0]);
    }
    else if (equalText(name, "ceil"))
    {
        REQUIRE_ARGS(1);
        result = ceill(values[0]);
    }
    else if (equalText(name, "trunc") || equalText(name, "int"))
    {
        REQUIRE_ARGS(1);
        result = truncl(values[0]);
    }
    else if (equalText(name, "frac"))
    {
        REQUIRE_ARGS(1);
        result = values[0] - truncl(values[0]);
    }
    else if (equalText(name, "round"))
    {
        REQUIRE_ARGS(1);
        result = roundl(values[0]);
    }
    else if (equalText(name, "sec") || equalText(name, "csc") ||
             equalText(name, "cot"))
    {
        long double radians;
        long double denominator;
        REQUIRE_ARGS(1);
        radians = angleToRadians(state->context, values[0]);
        denominator = equalText(name, "sec") ? cosl(radians) :
                      (equalText(name, "csc") ? sinl(radians) : tanl(radians));
        if (absoluteValue(denominator) < 1.0e-12L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = 1.0L / denominator;
    }
    else if (equalText(name, "sech") || equalText(name, "csch") ||
             equalText(name, "coth"))
    {
        long double denominator;
        REQUIRE_ARGS(1);
        denominator = equalText(name, "sech") ? coshl(values[0]) :
                      (equalText(name, "csch") ? sinhl(values[0]) :
                                                 tanhl(values[0]));
        if (absoluteValue(denominator) < 1.0e-18L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = 1.0L / denominator;
    }
    else if (equalText(name, "deg"))
    {
        REQUIRE_ARGS(1);
        result = values[0] * 180.0L / CALC_PI;
    }
    else if (equalText(name, "rad"))
    {
        REQUIRE_ARGS(1);
        result = values[0] * CALC_PI / 180.0L;
    }
    else if (equalText(name, "grad"))
    {
        REQUIRE_ARGS(1);
        result = values[0] * 200.0L / 180.0L;
    }
    else if (equalText(name, "ranint") || equalText(name, "ranint#"))
    {
        s64 span;
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) ||
            !integerValue(values[1], &secondInteger) ||
            secondInteger < firstInteger)
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            span = secondInteger - firstInteger + 1;
            if (span <= 0 || span > 0x100000000LL)
                setError(state, CALC_ERR_RANGE);
            else
                result = (long double)(firstInteger +
                         (s64)((u64)nextRandom(state->context) % (u64)span));
        }
    }
    else if (equalText(name, "normalpdf"))
    {
        long double z;
        REQUIRE_ARGS(3);
        if (values[2] <= 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            z = (values[0] - values[1]) / values[2];
            result = expl(-0.5L * z * z) /
                     (values[2] * sqrtl(2.0L * CALC_PI));
        }
    }
    else if (equalText(name, "normalcdf"))
    {
        REQUIRE_ARGS(3);
        if (values[2] <= 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = normalCdfStandard((values[0] - values[1]) / values[2]);
    }
    else if (equalText(name, "normalinv"))
    {
        REQUIRE_ARGS(3);
        if (values[0] <= 0.0L || values[0] >= 1.0L || values[2] <= 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else
            result = values[1] + values[2] * normalInverseStandard(values[0]);
    }
    else if (equalText(name, "binompdf") || equalText(name, "binomcdf"))
    {
        s64 trial;
        s64 success;
        u8 valid;
        REQUIRE_ARGS(3);
        if (!integerValue(values[0], &trial) ||
            !integerValue(values[2], &success) || trial < 0 ||
            values[1] < 0.0L || values[1] > 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else if (equalText(name, "binompdf"))
        {
            if (success < 0 || success > trial)
                result = 0.0L;
            else
                result = combination(trial, success, &valid) *
                         powl(values[1], (long double)success) *
                         powl(1.0L - values[1], (long double)(trial - success));
        }
        else
        {
            s64 k;
            result = 0.0L;
            if (success > trial)
                success = trial;
            for (k = 0; k <= success; ++k)
                result += combination(trial, k, &valid) *
                          powl(values[1], (long double)k) *
                          powl(1.0L - values[1], (long double)(trial - k));
        }
    }
    else if (equalText(name, "poissonpdf") || equalText(name, "poissoncdf"))
    {
        s64 k;
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) || values[1] < 0.0L)
            setError(state, CALC_ERR_DOMAIN);
        else if (firstInteger < 0)
            result = 0.0L;
        else if (equalText(name, "poissonpdf"))
        {
            result = expl(-values[1]);
            for (k = 1; k <= firstInteger; ++k)
                result *= values[1] / (long double)k;
        }
        else
        {
            long double term = expl(-values[1]);
            result = term;
            for (k = 1; k <= firstInteger; ++k)
            {
                term *= values[1] / (long double)k;
                result += term;
            }
        }
    }
    else if (equalText(name, "geometricpdf") || equalText(name, "geometriccdf"))
    {
        REQUIRE_ARGS(2);
        if (!integerValue(values[0], &firstInteger) || firstInteger < 1 ||
            values[1] <= 0.0L || values[1] > 1.0L)
            setError(state, CALC_ERR_DOMAIN);
        else if (equalText(name, "geometricpdf"))
            result = powl(1.0L - values[1], (long double)(firstInteger - 1)) *
                     values[1];
        else
            result = 1.0L - powl(1.0L - values[1], (long double)firstInteger);
    }
    else if (equalText(name, "hypergeometric") ||
             equalText(name, "hypergeompdf"))
    {
        s64 population;
        s64 successes;
        s64 draws;
        s64 observed;
        u8 validA;
        u8 validB;
        u8 validC;
        REQUIRE_ARGS(4);
        if (!integerValue(values[0], &population) ||
            !integerValue(values[1], &successes) ||
            !integerValue(values[2], &draws) ||
            !integerValue(values[3], &observed) || population < 0 ||
            successes < 0 || successes > population || draws < 0 ||
            draws > population)
            setError(state, CALC_ERR_DOMAIN);
        else
        {
            result = combination(successes, observed, &validA) *
                     combination(population - successes, draws - observed,
                                 &validB) /
                     combination(population, draws, &validC);
            if (!validA || !validB || !validC)
                result = 0.0L;
        }
    }
    else
        setError(state, CALC_ERR_SYNTAX);

#undef REQUIRE_ARGS
    if (!state->error && !finiteValue(result))
        setError(state, CALC_ERR_RANGE);
    return state->error ? 0.0L : result;
}

static long double evaluateIdentifier(EvalState *state,
                                      const CalcSyntaxNode *node)
{
    char name;
    u8 index;

    if (strcmp(node->text, "pi") == 0 || strcmp(node->text, "PI") == 0 ||
        strcmp(node->text, "Pi") == 0)
        return CALC_PI;
    if (strcmp(node->text, "e") == 0)
        return CALC_E;
    if (equalText(node->text, "ans"))
        return state->context->hasAnswer
                   ? calcNumberToLongDouble(state->context->answer)
                   : 0.0L;
    if (equalText(node->text, "preans"))
        return state->context->hasPreviousAnswer
                   ? calcNumberToLongDouble(state->context->previousAnswer)
                   : 0.0L;
    if (equalText(node->text, "ran#"))
        return randomUnit(state->context);
    if (node->text[0] == '\0' || node->text[1] != '\0')
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    name = lowerAscii(node->text[0]);
    if (name < 'a' || name > 'z')
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    index = (u8)(name - 'a');
    if (state->bindingMask & ((u32)1U << index))
        return state->bindings[index];
    if (state->context->variableMask & ((u32)1U << index))
        return calcNumberToLongDouble(state->context->variables[index]);
    return 0.0L;
}

static long double evaluateNode(EvalState *state, s16 nodeIndex)
{
    const CalcSyntaxNode *node;
    long double left;
    long double right;
    long double result = 0.0L;
    s64 integer;

    if (state->error)
        return 0.0L;
    if (nodeIndex < 0 || nodeIndex >= state->ast->count ||
        state->depth >= CALC_EVAL_MAX_DEPTH)
    {
        setError(state, CALC_ERR_SYNTAX);
        return 0.0L;
    }
    ++state->depth;
    node = &state->ast->nodes[nodeIndex];
    switch (node->kind)
    {
        case CALC_SYNTAX_NUMBER:
            result = parseNumberText(node->text, &state->error);
            break;
        case CALC_SYNTAX_IDENTIFIER:
            result = evaluateIdentifier(state, node);
            break;
        case CALC_SYNTAX_UNARY:
            result = evaluateNode(state, node->left);
            if (!state->error && node->op == '-')
                result = -result;
            else if (!state->error && node->op != '+')
                setError(state, CALC_ERR_SYNTAX);
            break;
        case CALC_SYNTAX_POSTFIX:
            left = evaluateNode(state, node->left);
            if (state->error)
                break;
            if (node->op == '%')
                result = left / 100.0L;
            else if (node->op == '!')
            {
                s64 index;
                if (!integerValue(left, &integer) || integer < 0 ||
                    integer > 100000)
                    setError(state, CALC_ERR_DOMAIN);
                else
                {
                    result = 1.0L;
                    for (index = 2; index <= integer; ++index)
                        result *= (long double)index;
                }
            }
            else
                setError(state, CALC_ERR_SYNTAX);
            break;
        case CALC_SYNTAX_BINARY:
            left = evaluateNode(state, node->left);
            right = evaluateNode(state, node->right);
            if (state->error)
                break;
            if (node->op == '+')
                result = left + right;
            else if (node->op == '-')
                result = left - right;
            else if (node->op == '*')
                result = left * right;
            else if (node->op == '/')
            {
                if (right == 0.0L)
                    setError(state, CALC_ERR_DIVZERO);
                else
                    result = left / right;
            }
            else if (node->op == '^')
            {
                if (left == 0.0L && right < 0.0L)
                    setError(state, CALC_ERR_DIVZERO);
                else if (left < 0.0L && !integerValue(right, &integer))
                    setError(state, CALC_ERR_POWER);
                else
                    result = powl(left, right);
            }
            else
                setError(state, CALC_ERR_SYNTAX);
            break;
        case CALC_SYNTAX_RELATION:
            left = evaluateNode(state, node->left);
            right = evaluateNode(state, node->right);
            if (state->error)
                break;
            if (node->op == '=')
                result = (long double)(left == right);
            else if (node->op == '!')
                result = (long double)(left != right);
            else if (node->op == '<')
                result = (long double)(left < right);
            else if (node->op == '>')
                result = (long double)(left > right);
            else if (node->op == 'L')
                result = (long double)(left <= right);
            else if (node->op == 'G')
                result = (long double)(left >= right);
            else
                setError(state, CALC_ERR_SYNTAX);
            break;
        case CALC_SYNTAX_CALL:
            result = evaluateFunction(state, node);
            break;
        default:
            setError(state, CALC_ERR_SYNTAX);
            break;
    }
    --state->depth;
    if (!state->error && !finiteValue(result))
        setError(state, CALC_ERR_RANGE);
    return state->error ? 0.0L : result;
}

static void evaluateExpression(CalcContext *context, const char *expression,
                               u8 bindX, CalcNumber xValue, u8 updateAnswer,
                               CalcNumber *result, u8 *error)
{
    const CalcSyntaxAst *ast;
    EvalState state;
    long double value;
    u8 conversionError;

    if (result != NULL)
    {
        result->mantissa = 0;
        result->exponent = 0;
    }
    if (error != NULL)
        *error = CALC_OK;
    if (context == NULL || expression == NULL || result == NULL || error == NULL)
    {
        if (error != NULL)
            *error = CALC_ERR_SYNTAX;
        return;
    }
    ast = cachedSyntaxAst(expression);
    if (ast == NULL)
    {
        *error = CALC_ERR_SYNTAX;
        return;
    }
    memset(&state, 0, sizeof(state));
    state.context = context;
    state.ast = ast;
    if (bindX)
    {
        state.bindingMask |= (u32)1U << ('x' - 'a');
        state.bindings['x' - 'a'] = calcNumberToLongDouble(xValue);
    }
    value = evaluateNode(&state, ast->root);
    if (state.error)
    {
        *error = state.error;
        return;
    }
    *result = calcNumberFromLongDouble(value, &conversionError);
    if (conversionError)
    {
        *error = conversionError;
        return;
    }
    if (updateAnswer)
    {
        if (context->hasAnswer)
        {
            context->previousAnswer = context->answer;
            context->hasPreviousAnswer = 1;
        }
        context->answer = *result;
        context->hasAnswer = 1;
    }
    *error = CALC_OK;
}

void calcEvaluateContext(CalcContext *context, const char *expression,
                         CalcNumber *result, u8 *error)
{
    CalcNumber unusedX;
    unusedX.mantissa = 0;
    unusedX.exponent = 0;
    evaluateExpression(context, expression, 0, unusedX, 1, result, error);
}

void calcEvalNumberContext(CalcContext *context, const char *expression,
                           CalcNumber xValue, CalcNumber *result, u8 *error)
{
    evaluateExpression(context, expression, 1, xValue, 0, result, error);
}

void calcClearVariables(void)
{
    calcContextInit(&defaultContext);
    defaultContextReady = 1;
}

void calcSetAngleMode(u8 mode)
{
    ensureDefaultContext();
    if (mode <= CALC_ANGLE_GRAD)
        defaultContext.angleMode = mode;
}

u8 calcGetAngleMode(void)
{
    ensureDefaultContext();
    return defaultContext.angleMode;
}

u8 calcSetVariable(char name, CalcNumber value)
{
    ensureDefaultContext();
    return calcContextSetVariable(&defaultContext, name, value);
}

u8 calcGetVariable(char name, CalcNumber *value)
{
    ensureDefaultContext();
    return calcContextGetVariable(&defaultContext, name, value);
}

void calcEvaluate(char *expression, CalcNumber *result, u8 *error)
{
    ensureDefaultContext();
    calcEvaluateContext(&defaultContext, expression, result, error);
}

void calcEvalNumber(char *expression, CalcNumber xValue,
                    CalcNumber *result, u8 *error)
{
    ensureDefaultContext();
    calcEvalNumberContext(&defaultContext, expression, xValue, result, error);
}

static void appendCharacter(char *buffer, u8 capacity, u8 *length,
                            char character)
{
    if (*length + 1 < capacity)
    {
        buffer[*length] = character;
        ++*length;
        buffer[*length] = '\0';
    }
}

static void appendExponent(char *buffer, u8 capacity, u8 *length, s16 value)
{
    char digits[6];
    u8 count = 0;
    u16 magnitude;

    if (value < 0)
    {
        appendCharacter(buffer, capacity, length, '-');
        magnitude = (u16)-value;
    }
    else
        magnitude = (u16)value;
    do
    {
        digits[count++] = (char)('0' + magnitude % 10U);
        magnitude = (u16)(magnitude / 10U);
    } while (magnitude && count < sizeof(digits));
    while (count)
        appendCharacter(buffer, capacity, length, digits[--count]);
}

void calcFormatNumber(CalcNumber value, char *buffer, u8 bufferSize)
{
    char digits[9];
    char temporary[48];
    u32 magnitude;
    s16 decimalPosition;
    s8 last;
    u8 length = 0;
    u8 index;

    if (buffer == NULL || bufferSize == 0)
        return;
    buffer[0] = '\0';
    temporary[0] = '\0';
    if (value.mantissa == 0)
    {
        if (bufferSize > 1)
        {
            buffer[0] = '0';
            buffer[1] = '\0';
        }
        return;
    }
    if (value.mantissa < 0)
    {
        appendCharacter(temporary, sizeof(temporary), &length, '-');
        magnitude = (u32)(-(s64)value.mantissa);
    }
    else
        magnitude = (u32)value.mantissa;
    for (index = 9; index > 0; --index)
    {
        digits[index - 1] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    }
    last = 8;
    while (last > 0 && digits[(u8)last] == '0')
        --last;
    decimalPosition = (s16)(value.exponent + 1);
    if (value.exponent >= 9 || value.exponent <= -5)
    {
        appendCharacter(temporary, sizeof(temporary), &length, digits[0]);
        if (last > 0)
        {
            appendCharacter(temporary, sizeof(temporary), &length, '.');
            for (index = 1; index <= (u8)last; ++index)
                appendCharacter(temporary, sizeof(temporary), &length,
                                digits[index]);
        }
        appendCharacter(temporary, sizeof(temporary), &length, 'E');
        appendExponent(temporary, sizeof(temporary), &length, value.exponent);
    }
    else if (decimalPosition <= 0)
    {
        appendCharacter(temporary, sizeof(temporary), &length, '0');
        appendCharacter(temporary, sizeof(temporary), &length, '.');
        for (index = 0; index < (u8)-decimalPosition; ++index)
            appendCharacter(temporary, sizeof(temporary), &length, '0');
        for (index = 0; index <= (u8)last; ++index)
            appendCharacter(temporary, sizeof(temporary), &length,
                            digits[index]);
    }
    else
    {
        for (index = 0; index < (u8)decimalPosition; ++index)
        {
            appendCharacter(temporary, sizeof(temporary), &length,
                            index < 9 ? digits[index] : '0');
        }
        if (decimalPosition <= last)
        {
            appendCharacter(temporary, sizeof(temporary), &length, '.');
            for (index = (u8)decimalPosition; index <= (u8)last; ++index)
                appendCharacter(temporary, sizeof(temporary), &length,
                                digits[index]);
        }
    }
    for (index = 0; index + 1 < bufferSize && temporary[index]; ++index)
        buffer[index] = temporary[index];
    buffer[index] = '\0';
}

u8 calcTangentFixed(s32 phaseFixed, s32 *fixedValue)
{
    long double phase;
    long double radians;
    long double cosine;
    long double scaled;

    ensureDefaultContext();
    if (fixedValue == NULL)
        return CALC_ERR_RANGE;
    phase = (long double)phaseFixed / (long double)CALC_ONE;
    radians = angleToRadians(&defaultContext, phase);
    cosine = cosl(radians);
    if (absoluteValue(cosine) < 1.0e-12L)
        return CALC_ERR_DOMAIN;
    scaled = tanl(radians) * (long double)CALC_ONE;
    if (!finiteValue(scaled) || scaled > (long double)INT32_MAX ||
        scaled < (long double)INT32_MIN)
        return CALC_ERR_RANGE;
    *fixedValue = (s32)(scaled < 0.0L ? ceill(scaled - 0.5L) :
                                      floorl(scaled + 0.5L));
    return CALC_OK;
}

u8 calcTangentPhaseCrosses(CalcNumber phase0, CalcNumber phase1)
{
    long double left;
    long double right;
    long double low;
    long double high;
    long double firstIndex;
    long double pole;

    ensureDefaultContext();
    left = angleToRadians(&defaultContext, calcNumberToLongDouble(phase0));
    right = angleToRadians(&defaultContext, calcNumberToLongDouble(phase1));
    low = left < right ? left : right;
    high = left < right ? right : left;
    firstIndex = ceill((low - CALC_PI * 0.5L) / CALC_PI);
    pole = CALC_PI * 0.5L + firstIndex * CALC_PI;
    return (u8)(pole >= low && pole <= high);
}

s32 calcEval(char *expression, s32 xValue, u8 *error)
{
    CalcNumber result;
    s32 fixed = 0;

    ensureDefaultContext();
    calcEvalNumberContext(&defaultContext, expression,
                          calcNumberFromFixed(xValue), &result, error);
    if (*error == CALC_OK)
        *error = calcNumberToFixed(result, &fixed);
    return fixed;
}

const char *calcErrorText(u8 error)
{
    switch (error)
    {
        case CALC_OK:
            return "OK";
        case CALC_ERR_SYNTAX:
            return "Syntax ERROR";
        case CALC_ERR_DIVZERO:
            return "Divide by zero";
        case CALC_ERR_DOMAIN:
            return "Math ERROR";
        case CALC_ERR_POWER:
            return "Power ERROR";
        case CALC_ERR_RANGE:
            return "Range ERROR";
        default:
            return "ERROR";
    }
}
