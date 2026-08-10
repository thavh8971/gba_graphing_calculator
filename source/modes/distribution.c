#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

static long double normalPdf(long double x, long double mean,
                             long double deviation)
{
    long double z = (x - mean) / deviation;
    return expl(-0.5L * z * z) /
           (deviation * sqrtl(2.0L * 3.1415926535897932384626L));
}

static long double normalCdf(long double x, long double mean,
                             long double deviation)
{
    return 0.5L * (1.0L + erfl((x - mean) /
                               (deviation * sqrtl(2.0L))));
}

/* Peter J. Acklam's rational approximation, followed by two Newton steps. */
static long double inverseStandardNormal(long double probability)
{
    static const long double a[] = {
        -3.969683028665376e+01L, 2.209460984245205e+02L,
        -2.759285104469687e+02L, 1.383577518672690e+02L,
        -3.066479806614716e+01L, 2.506628277459239e+00L
    };
    static const long double b[] = {
        -5.447609879822406e+01L, 1.615858368580409e+02L,
        -1.556989798598866e+02L, 6.680131188771972e+01L,
        -1.328068155288572e+01L
    };
    static const long double c[] = {
        -7.784894002430293e-03L, -3.223964580411365e-01L,
        -2.400758277161838e+00L, -2.549732539343734e+00L,
        4.374664141464968e+00L, 2.938163982698783e+00L
    };
    static const long double d[] = {
        7.784695709041462e-03L, 3.224671290700398e-01L,
        2.445134137142996e+00L, 3.754408661907416e+00L
    };
    const long double low = 0.02425L;
    const long double high = 1.0L - low;
    long double q;
    long double r;
    long double x;
    u8 iteration;

    if (probability < low) {
        q = sqrtl(-2.0L * logl(probability));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q +
              c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0L);
    } else if (probability <= high) {
        q = probability - 0.5L;
        r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r +
              a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r +
              b[4]) * r + 1.0L);
    } else {
        q = sqrtl(-2.0L * logl(1.0L - probability));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q +
               c[4]) * q + c[5]) /
             ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1.0L);
    }
    for (iteration = 0; iteration < 2; iteration++) {
        long double error = normalCdf(x, 0.0L, 1.0L) - probability;
        x -= error / normalPdf(x, 0.0L, 1.0L);
    }
    return x;
}

static long double binomialPdf(s32 n, long double p, s32 k)
{
    return modeCombination(n, k) * powl(p, k) * powl(1.0L - p, n - k);
}

static long double binomialCdf(s32 n, long double p, s32 k)
{
    s32 index;
    long double term;
    long double result;

    if (p == 0.0L)
        return 1.0L;
    if (p == 1.0L)
        return k >= n ? 1.0L : 0.0L;
    if (k >= n)
        return 1.0L;
    /* Sum from the numerically safer tail.  Each term is derived from the
       previous one, avoiding a combination and two powers per sample. */
    if (p <= 0.5L) {
        term = powl(1.0L - p, n);
        result = term;
        for (index = 1; index <= k; index++) {
            term *= ((long double)(n - index + 1) / index) *
                    (p / (1.0L - p));
            result += term;
        }
        return result;
    }
    term = powl(p, n);
    result = term;
    for (index = 1; index <= n - k - 1; index++) {
        term *= ((long double)(n - index + 1) / index) *
                ((1.0L - p) / p);
        result += term;
    }
    return 1.0L - result;
}

static long double poissonPdf(long double lambda, s32 k)
{
    s32 index;
    long double result = expl(-lambda);
    for (index = 1; index <= k; index++)
        result *= lambda / index;
    return result;
}

static long double poissonCdf(long double lambda, s32 k)
{
    s32 index;
    long double term = expl(-lambda);
    long double result = term;
    for (index = 1; index <= k; index++) {
        term *= lambda / index;
        result += term;
    }
    return result;
}

static u8 integerParameter(long double value, s32 *integer)
{
    if (!isfinite((double)value) || value < -2147483648.0L ||
        value > 2147483647.0L || value != truncl(value))
        return 0;
    *integer = (s32)value;
    return 1;
}

u8 modeDistributionEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[24];
    char arguments[192];
    const char *body = expression;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));
    long double values[4];
    u8 count;
    u8 error;
    long double result;
    char output[48];

    if (command)
        body = arguments;
    error = modeParseRealList(runtime, body, ';', values, 4, &count);
    if (error != CALC_OK)
        return error;

    if (!command || modeEquals(name, "binom")) {
        s32 n;
        s32 k;
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (!integerParameter(values[0], &n) ||
            !integerParameter(values[2], &k) || n < 0 || k < 0 || k > n ||
            values[1] < 0.0L || values[1] > 1.0L)
            return CALC_ERR_DOMAIN;
        result = binomialPdf(n, values[1], k);
    } else if (modeEquals(name, "binompdf")) {
        s32 k;
        s32 n;
        const char *colon = strchr(expression, ':');
        const char *open = strchr(expression, '(');
        u8 legacy = colon != 0 && (open == 0 || colon < open);
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (legacy) {
            if (!integerParameter(values[0], &n) ||
                !integerParameter(values[2], &k) || n < 0 || k < 0 ||
                k > n || values[1] < 0.0L || values[1] > 1.0L)
                return CALC_ERR_DOMAIN;
            result = binomialPdf(n, values[1], k);
        } else {
            if (!integerParameter(values[0], &k) ||
                !integerParameter(values[1], &n) || n < 0 || k < 0 ||
                k > n || values[2] < 0.0L || values[2] > 1.0L)
                return CALC_ERR_DOMAIN;
            result = binomialPdf(n, values[2], k);
        }
    } else if (modeEquals(name, "binomcdf")) {
        s32 n;
        s32 k;
        const char *colon = strchr(expression, ':');
        const char *open = strchr(expression, '(');
        u8 legacy = colon != 0 && (open == 0 || colon < open);
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (legacy) {
            if (!integerParameter(values[0], &n) ||
                !integerParameter(values[2], &k) || n < 0 || k < 0 ||
                k > n || values[1] < 0.0L || values[1] > 1.0L)
                return CALC_ERR_DOMAIN;
            result = binomialCdf(n, values[1], k);
        } else {
            if (!integerParameter(values[0], &k) ||
                !integerParameter(values[1], &n) || n < 0 || k < 0 ||
                k > n || values[2] < 0.0L || values[2] > 1.0L)
                return CALC_ERR_DOMAIN;
            result = binomialCdf(n, values[2], k);
        }
    } else if (modeEquals(name, "poisson") ||
               modeEquals(name, "poissonpdf")) {
        s32 k;
        if (count != 2)
            return CALC_ERR_SYNTAX;
        /* Keep PDF and CDF consistent with the evaluator: k;lambda.  Guessing
           the order from which operand looks integral reverses common inputs
           such as 3;2, where both operands are integers. */
        if (!integerParameter(values[0], &k) || k < 0 || values[1] < 0.0L)
            return CALC_ERR_DOMAIN;
        result = poissonPdf(values[1], k);
    } else if (modeEquals(name, "poissoncdf")) {
        s32 k;
        long double lambda;
        if (count != 2)
            return CALC_ERR_SYNTAX;
        lambda = values[1];
        if (!integerParameter(values[0], &k) || k < 0 || lambda < 0.0L)
            return CALC_ERR_DOMAIN;
        result = poissonCdf(lambda, k);
    } else if (modeEquals(name, "normpdf")) {
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (values[1] <= 0.0L)
            return CALC_ERR_DOMAIN;
        result = normalPdf(values[0], values[2], values[1]);
    } else if (modeEquals(name, "normcdf")) {
        if (count != 4)
            return CALC_ERR_SYNTAX;
        if (values[2] <= 0.0L || values[0] > values[1])
            return CALC_ERR_DOMAIN;
        result = normalCdf(values[1], values[3], values[2]) -
                 normalCdf(values[0], values[3], values[2]);
    } else if (modeEquals(name, "norminv")) {
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (values[0] <= 0.0L || values[0] >= 1.0L || values[1] <= 0.0L)
            return CALC_ERR_DOMAIN;
        result = values[2] + values[1] * inverseStandardNormal(values[0]);
    } else if (modeEquals(name, "normalpdf")) {
        /* Compatibility spelling/order: x;mean;standard-deviation. */
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (values[2] <= 0.0L)
            return CALC_ERR_DOMAIN;
        result = normalPdf(values[0], values[1], values[2]);
    } else if (modeEquals(name, "normal") ||
               modeEquals(name, "normalcdf")) {
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (values[2] <= 0.0L)
            return CALC_ERR_DOMAIN;
        result = normalCdf(values[0], values[1], values[2]);
    } else if (modeEquals(name, "normalinv")) {
        if (count != 3)
            return CALC_ERR_SYNTAX;
        if (values[0] <= 0.0L || values[0] >= 1.0L || values[2] <= 0.0L)
            return CALC_ERR_DOMAIN;
        result = values[1] + values[2] * inverseStandardNormal(values[0]);
    } else if (modeEquals(name, "geompdf") ||
               modeEquals(name, "geometricpdf")) {
        s32 k;
        if (count != 2)
            return CALC_ERR_SYNTAX;
        if (!integerParameter(values[0], &k) || k < 1 ||
            values[1] <= 0.0L || values[1] > 1.0L)
            return CALC_ERR_DOMAIN;
        result = powl(1.0L - values[1], k - 1) * values[1];
    } else if (modeEquals(name, "geomcdf") ||
               modeEquals(name, "geometriccdf")) {
        s32 k;
        if (count != 2)
            return CALC_ERR_SYNTAX;
        if (!integerParameter(values[0], &k) || k < 1 ||
            values[1] <= 0.0L || values[1] > 1.0L)
            return CALC_ERR_DOMAIN;
        result = 1.0L - powl(1.0L - values[1], k);
    } else if (modeEquals(name, "hypergeom") ||
               modeEquals(name, "hypergeometric") ||
               modeEquals(name, "hypergeompdf")) {
        s32 population;
        s32 success;
        s32 draws;
        s32 observed;
        if (count != 4)
            return CALC_ERR_SYNTAX;
        if (!integerParameter(values[0], &population) ||
            !integerParameter(values[1], &success) ||
            !integerParameter(values[2], &draws) ||
            !integerParameter(values[3], &observed) || population <= 0 ||
            success < 0 || success > population ||
            draws < 0 || draws > population || observed < 0 ||
            observed > success || draws - observed > population - success)
            return CALC_ERR_DOMAIN;
        result = modeCombination(success, observed) *
                 modeCombination(population - success, draws - observed) /
                 modeCombination(population, draws);
    } else
        return CALC_ERR_SYNTAX;
    modeFormatReal(result, output, sizeof(output), 12);
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}
