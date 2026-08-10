#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

typedef struct LinearSums {
    long double weight;
    long double x;
    long double y;
    long double xx;
    long double yy;
    long double xy;
} LinearSums;

typedef struct QuadraticSums {
    long double weight;
    long double x;
    long double x2;
    long double x3;
    long double x4;
    long double y;
    long double xy;
    long double x2y;
} QuadraticSums;

static void sortValues(long double *values, u8 count)
{
    u8 index;
    for (index = 1; index < count; index++) {
        long double value = values[index];
        s16 position = (s16)index - 1;
        while (position >= 0 && values[position] > value) {
            values[position + 1] = values[position];
            position--;
        }
        values[position + 1] = value;
    }
}

static u8 statBasic(ModeRuntime *runtime, const char *text,
                    const char *command)
{
    long double values[MODE_MAX_VALUES];
    long double sum = 0.0L;
    long double sumSquares = 0.0L;
    long double product = 1.0L;
    long double mean;
    long double variance = 0.0L;
    u8 count;
    u8 index;
    u8 error;
    char output[MODE_RESULT_CAPACITY];
    char first[32];
    char second[32];

    error = modeParseRealList(runtime, text, ';', values,
                              MODE_MAX_VALUES, &count);
    if (error != CALC_OK)
        return error;
    for (index = 0; index < count; index++) {
        sum += values[index];
        sumSquares += values[index] * values[index];
        product *= values[index];
    }
    mean = sum / count;
    for (index = 0; index < count; index++) {
        long double delta = values[index] - mean;
        variance += delta * delta;
    }
    if (command == 0 || modeEquals(command, "stat1var") ||
        modeEquals(command, "onevar")) {
        modeFormatReal(mean, first, sizeof(first), 9);
        modeFormatReal(sqrtl(variance / count), second, sizeof(second), 9);
        snprintf(output, sizeof(output), "N=%u M=%s SD=%s",
                 count, first, second);
    } else if (modeEquals(command, "sum"))
        modeFormatReal(sum, output, sizeof(output), 10);
    else if (modeEquals(command, "prod"))
        modeFormatReal(product, output, sizeof(output), 10);
    else if (modeEquals(command, "mean"))
        modeFormatReal(mean, output, sizeof(output), 10);
    else if (modeEquals(command, "sumx2"))
        modeFormatReal(sumSquares, output, sizeof(output), 10);
    else if (modeEquals(command, "min")) {
        long double minimum = values[0];
        for (index = 1; index < count; index++)
            if (values[index] < minimum)
                minimum = values[index];
        modeFormatReal(minimum, output, sizeof(output), 10);
    } else if (modeEquals(command, "max")) {
        long double maximum = values[0];
        for (index = 1; index < count; index++)
            if (values[index] > maximum)
                maximum = values[index];
        modeFormatReal(maximum, output, sizeof(output), 10);
    } else if (modeEquals(command, "median")) {
        sortValues(values, count);
        if (count & 1)
            mean = values[count / 2];
        else
            mean = (values[count / 2 - 1] + values[count / 2]) / 2.0L;
        modeFormatReal(mean, output, sizeof(output), 10);
    } else if (modeEquals(command, "varp"))
        modeFormatReal(variance / count, output, sizeof(output), 10);
    else if (modeEquals(command, "vars") || modeEquals(command, "varsamp")) {
        if (count < 2)
            return CALC_ERR_DOMAIN;
        modeFormatReal(variance / (count - 1), output, sizeof(output), 10);
    } else if (modeEquals(command, "sdpop"))
        modeFormatReal(sqrtl(variance / count), output, sizeof(output), 10);
    else if (modeEquals(command, "sdsamp")) {
        if (count < 2)
            return CALC_ERR_DOMAIN;
        modeFormatReal(sqrtl(variance / (count - 1)), output,
                       sizeof(output), 10);
    } else if (modeEquals(command, "cuml")) {
        u16 used = 0;
        long double cumulative = 0.0L;
        output[0] = '\0';
        for (index = 0; index < count; index++) {
            char value[24];
            u16 length;
            cumulative += values[index];
            modeFormatReal(cumulative, value, sizeof(value), 8);
            length = (u16)strlen(value);
            if ((size_t)used + length + (index != 0 ? 1U : 0U) + 1U >=
                sizeof(output))
                break;
            if (index != 0)
                output[used++] = ',';
            memcpy(output + used, value, length + 1);
            used = (u16)(used + length);
        }
    } else
        return CALC_ERR_SYNTAX;
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static u8 finishOneVariable(ModeRuntime *runtime, long double total,
                            long double weighted,
                            long double weightedSquares)
{
    long double mean;
    long double variance;
    char countText[24];
    char meanText[32];
    char deviationText[32];
    char output[MODE_RESULT_CAPACITY];

    if (total <= 0.0L)
        return CALC_ERR_DOMAIN;
    mean = weighted / total;
    variance = weightedSquares / total - mean * mean;
    if (variance < 0.0L &&
        variance > -1e-12L * (1.0L + fabsl(weightedSquares / total) +
                              mean * mean))
        variance = 0.0L;
    if (variance < 0.0L || !isfinite((double)mean) ||
        !isfinite((double)variance))
        return CALC_ERR_RANGE;
    modeFormatReal(total, countText, sizeof(countText), 9);
    modeFormatReal(mean, meanText, sizeof(meanText), 9);
    modeFormatReal(sqrtl(variance), deviationText,
                   sizeof(deviationText), 9);
    snprintf(output, sizeof(output), "N=%s M=%s SD=%s",
             countText, meanText, deviationText);
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static u8 statFrequency(ModeRuntime *runtime, const char *body)
{
    char halves[2][64];
    long double values[MODE_MAX_VALUES];
    long double frequencies[MODE_MAX_VALUES];
    long double total = 0.0L;
    long double weighted = 0.0L;
    long double weightedSquares = 0.0L;
    u8 halvesCount;
    u8 valueCount;
    u8 frequencyCount;
    u8 index;
    u8 error;

    if (!modeSplit(body, ';', halves, 2, &halvesCount) || halvesCount != 2)
        return CALC_ERR_SYNTAX;
    error = modeParseRealList(runtime, halves[0], ',', values,
                              MODE_MAX_VALUES, &valueCount);
    if (error != CALC_OK)
        return error;
    error = modeParseRealList(runtime, halves[1], ',', frequencies,
                              MODE_MAX_VALUES, &frequencyCount);
    if (error != CALC_OK || frequencyCount != valueCount)
        return error == CALC_OK ? CALC_ERR_SYNTAX : error;
    for (index = 0; index < valueCount; index++) {
        if (frequencies[index] < 0.0L ||
            frequencies[index] != truncl(frequencies[index]))
            return CALC_ERR_DOMAIN;
        total += frequencies[index];
        weighted += values[index] * frequencies[index];
        weightedSquares += values[index] * values[index] * frequencies[index];
    }
    return finishOneVariable(runtime, total, weighted, weightedSquares);
}

static u8 transformRegression(StatModel model, long double x, long double y,
                              long double *transformedX,
                              long double *transformedY)
{
    switch (model) {
    case STAT_MODEL_LINEAR:
        *transformedX = x;
        *transformedY = y;
        break;
    case STAT_MODEL_LOGARITHMIC:
        if (x <= 0.0L)
            return CALC_ERR_DOMAIN;
        *transformedX = logl(x);
        *transformedY = y;
        break;
    case STAT_MODEL_EXPONENTIAL:
    case STAT_MODEL_AB_EXPONENTIAL:
        if (y <= 0.0L)
            return CALC_ERR_DOMAIN;
        *transformedX = x;
        *transformedY = logl(y);
        break;
    case STAT_MODEL_POWER:
        if (x <= 0.0L || y <= 0.0L)
            return CALC_ERR_DOMAIN;
        *transformedX = logl(x);
        *transformedY = logl(y);
        break;
    case STAT_MODEL_INVERSE:
        if (x == 0.0L)
            return CALC_ERR_DOMAIN;
        *transformedX = 1.0L / x;
        *transformedY = y;
        break;
    default:
        return CALC_ERR_SYNTAX;
    }
    return (isfinite((double)*transformedX) &&
            isfinite((double)*transformedY)) ? CALC_OK : CALC_ERR_RANGE;
}

static u8 addRegressionObservation(StatModel model, long double x,
                                   long double y, long double weight,
                                   LinearSums *linear,
                                   QuadraticSums *quadratic)
{
    long double x2;

    if (weight < 0.0L || weight != truncl(weight))
        return CALC_ERR_DOMAIN;
    if (weight == 0.0L)
        return CALC_OK;
    if (model == STAT_MODEL_QUADRATIC) {
        x2 = x * x;
        quadratic->weight += weight;
        quadratic->x += weight * x;
        quadratic->x2 += weight * x2;
        quadratic->x3 += weight * x2 * x;
        quadratic->x4 += weight * x2 * x2;
        quadratic->y += weight * y;
        quadratic->xy += weight * x * y;
        quadratic->x2y += weight * x2 * y;
        if (!isfinite((double)quadratic->x4) ||
            !isfinite((double)quadratic->x2y))
            return CALC_ERR_RANGE;
    } else {
        long double transformedX;
        long double transformedY;
        u8 error = transformRegression(model, x, y, &transformedX,
                                       &transformedY);
        if (error != CALC_OK)
            return error;
        linear->weight += weight;
        linear->x += weight * transformedX;
        linear->y += weight * transformedY;
        linear->xx += weight * transformedX * transformedX;
        linear->yy += weight * transformedY * transformedY;
        linear->xy += weight * transformedX * transformedY;
        if (!isfinite((double)linear->xx) ||
            !isfinite((double)linear->yy) ||
            !isfinite((double)linear->xy))
            return CALC_ERR_RANGE;
    }
    return CALC_OK;
}

static u8 parseRegressionRows(ModeRuntime *runtime, const char *body,
                              StatModel model, LinearSums *linear,
                              QuadraticSums *quadratic)
{
    char rows[MODE_MAX_VALUES][64];
    u8 rowCount;
    u8 row;

    memset(linear, 0, sizeof(*linear));
    memset(quadratic, 0, sizeof(*quadratic));
    if (!modeSplit(body, ';', rows, MODE_MAX_VALUES, &rowCount) ||
        rowCount < 2)
        return CALC_ERR_SYNTAX;
    for (row = 0; row < rowCount; row++) {
        long double fields[3];
        long double x;
        long double y;
        long double weight = 1.0L;
        u8 count;
        u8 error = modeParseRealList(runtime, rows[row], ',', fields, 3,
                                     &count);
        if (error != CALC_OK || (count != 2 && count != 3))
            return error == CALC_OK ? CALC_ERR_SYNTAX : error;
        x = fields[0];
        y = fields[1];
        if (count == 3) {
            weight = fields[2];
        }
        error = addRegressionObservation(model, x, y, weight, linear,
                                         quadratic);
        if (error != CALC_OK)
            return error;
    }
    return (model == STAT_MODEL_QUADRATIC ? quadratic->weight :
            linear->weight) > 0.0L ? CALC_OK : CALC_ERR_DOMAIN;
}

static u8 solveThreeByThree(long double matrix[3][4], long double answer[3])
{
    u8 column;
    u8 row;

    for (column = 0; column < 3; column++) {
        u8 best = column;
        long double scale = 0.0L;
        long double divisor;
        for (row = column; row < 3; row++) {
            u8 item;
            for (item = column; item < 3; item++)
                if (fabsl(matrix[row][item]) > scale)
                    scale = fabsl(matrix[row][item]);
            if (fabsl(matrix[row][column]) > fabsl(matrix[best][column]))
                best = row;
        }
        if (scale == 0.0L || fabsl(matrix[best][column]) <= scale * 1e-18L)
            return CALC_ERR_DOMAIN;
        if (best != column) {
            u8 item;
            for (item = column; item < 4; item++) {
                long double swap = matrix[column][item];
                matrix[column][item] = matrix[best][item];
                matrix[best][item] = swap;
            }
        }
        divisor = matrix[column][column];
        for (row = (u8)(column + 1); row < 3; row++) {
            long double factor = matrix[row][column] / divisor;
            u8 item;
            for (item = column; item < 4; item++)
                matrix[row][item] -= factor * matrix[column][item];
        }
    }
    for (row = 3; row-- > 0;) {
        long double value = matrix[row][3];
        u8 item;
        for (item = (u8)(row + 1); item < 3; item++)
            value -= matrix[row][item] * answer[item];
        answer[row] = value / matrix[row][row];
    }
    return CALC_OK;
}

static u8 finishRegression(ModeRuntime *runtime, StatModel model,
                           const LinearSums *sums,
                           const QuadraticSums *quadratic)
{
    long double a;
    long double b;
    long double c = 0.0L;
    long double correlation = 0.0L;
    u8 error;
    char countText[24];
    char aText[24];
    char bText[24];
    char cText[24];
    char rText[24];
    char output[MODE_RESULT_CAPACITY];

    if (model == STAT_MODEL_QUADRATIC) {
        long double matrix[3][4] = {
            {quadratic->weight, quadratic->x, quadratic->x2, quadratic->y},
            {quadratic->x, quadratic->x2, quadratic->x3, quadratic->xy},
            {quadratic->x2, quadratic->x3, quadratic->x4, quadratic->x2y}
        };
        long double answer[3] = {0.0L, 0.0L, 0.0L};
        error = solveThreeByThree(matrix, answer);
        if (error != CALC_OK)
            return error;
        a = answer[0];
        b = answer[1];
        c = answer[2];
    } else {
        long double centeredXX = sums->xx - sums->x * sums->x / sums->weight;
        long double centeredYY = sums->yy - sums->y * sums->y / sums->weight;
        long double centeredXY = sums->xy - sums->x * sums->y / sums->weight;
        long double xxTolerance = 1e-15L *
            (1.0L + fabsl(sums->xx) +
             fabsl(sums->x * sums->x / sums->weight));
        long double yyTolerance = 1e-15L *
            (1.0L + fabsl(sums->yy) +
             fabsl(sums->y * sums->y / sums->weight));
        long double intercept;
        long double slope;

        if (centeredXX <= xxTolerance)
            return CALC_ERR_DOMAIN;
        if (centeredYY < 0.0L && centeredYY >= -yyTolerance)
            centeredYY = 0.0L;
        if (centeredYY < 0.0L)
            return CALC_ERR_RANGE;
        slope = centeredXY / centeredXX;
        intercept = (sums->y - slope * sums->x) / sums->weight;
        if (centeredYY > yyTolerance) {
            correlation = centeredXY / sqrtl(centeredXX * centeredYY);
            if (correlation > 1.0L)
                correlation = 1.0L;
            else if (correlation < -1.0L)
                correlation = -1.0L;
        }
        if (model == STAT_MODEL_EXPONENTIAL) {
            a = expl(intercept);
            b = slope;
        } else if (model == STAT_MODEL_AB_EXPONENTIAL) {
            a = expl(intercept);
            b = expl(slope);
        } else if (model == STAT_MODEL_POWER) {
            a = expl(intercept);
            b = slope;
        } else {
            a = intercept;
            b = slope;
        }
        if (!isfinite((double)a) || !isfinite((double)b))
            return CALC_ERR_RANGE;
    }
    modeFormatReal(model == STAT_MODEL_QUADRATIC ? quadratic->weight :
                   sums->weight, countText, sizeof(countText), 9);
    modeFormatReal(a, aText, sizeof(aText), 9);
    modeFormatReal(b, bText, sizeof(bText), 9);
    if (model == STAT_MODEL_QUADRATIC) {
        modeFormatReal(c, cText, sizeof(cText), 9);
        snprintf(output, sizeof(output), "N=%s A=%s B=%s C=%s",
                 countText, aText, bText, cText);
    } else {
        modeFormatReal(correlation, rText, sizeof(rText), 9);
        snprintf(output, sizeof(output), "N=%s A=%s B=%s R=%s",
                 countText, aText, bText, rText);
    }
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static u8 statRegression(ModeRuntime *runtime, const char *body,
                         StatModel model)
{
    LinearSums sums;
    QuadraticSums quadratic;
    u8 error = parseRegressionRows(runtime, body, model, &sums, &quadratic);

    if (error != CALC_OK)
        return error;
    return finishRegression(runtime, model, &sums, &quadratic);
}

static u8 modelForCommand(const char *name, StatModel *model)
{
    if (modeEquals(name, "linear") || modeEquals(name, "linreg"))
        *model = STAT_MODEL_LINEAR;
    else if (modeEquals(name, "quadratic") || modeEquals(name, "quadreg"))
        *model = STAT_MODEL_QUADRATIC;
    else if (modeEquals(name, "logarithmic") || modeEquals(name, "logreg"))
        *model = STAT_MODEL_LOGARITHMIC;
    else if (modeEquals(name, "exponential") || modeEquals(name, "expreg"))
        *model = STAT_MODEL_EXPONENTIAL;
    else if (modeEquals(name, "abexp") || modeEquals(name, "abreg"))
        *model = STAT_MODEL_AB_EXPONENTIAL;
    else if (modeEquals(name, "power") || modeEquals(name, "powerreg"))
        *model = STAT_MODEL_POWER;
    else if (modeEquals(name, "inverse") || modeEquals(name, "invreg"))
        *model = STAT_MODEL_INVERSE;
    else
        return 0;
    return 1;
}

u8 modeStatEvaluateRows(ModeRuntime *runtime, StatModel model,
                        const char *const xValues[],
                        const char *const yValues[],
                        const char *const frequencies[], u8 count)
{
    LinearSums linear;
    QuadraticSums quadratic;
    long double total = 0.0L;
    long double weighted = 0.0L;
    long double weightedSquares = 0.0L;
    u8 row;
    u8 error = CALC_OK;

    if (runtime == 0)
        return CALC_ERR_SYNTAX;
    runtime->result[0] = '\0';
    runtime->error = CALC_OK;
    if ((u8)model >= STAT_MODEL_COUNT || xValues == 0 || count == 0) {
        error = CALC_ERR_SYNTAX;
        goto finish;
    }
    modeSetStatModel(runtime, model);
    memset(&linear, 0, sizeof(linear));
    memset(&quadratic, 0, sizeof(quadratic));
    if (model != STAT_MODEL_1VAR && yValues == 0) {
        error = CALC_ERR_SYNTAX;
        goto finish;
    }

    for (row = 0; row < count; row++) {
        long double x;
        long double y = 0.0L;
        long double weight = 1.0L;

        if (xValues[row] == 0 || xValues[row][0] == '\0') {
            error = CALC_ERR_SYNTAX;
            goto finish;
        }
        error = modeEvalReal(runtime, xValues[row], &x);
        if (error != CALC_OK)
            goto finish;
        if (model != STAT_MODEL_1VAR) {
            if (yValues[row] == 0 || yValues[row][0] == '\0') {
                error = CALC_ERR_SYNTAX;
                goto finish;
            }
            error = modeEvalReal(runtime, yValues[row], &y);
            if (error != CALC_OK)
                goto finish;
        }
        if (frequencies != 0 && frequencies[row] != 0 &&
            frequencies[row][0] != '\0') {
            error = modeEvalReal(runtime, frequencies[row], &weight);
            if (error != CALC_OK)
                goto finish;
        }
        if (weight < 0.0L || weight != truncl(weight)) {
            error = CALC_ERR_DOMAIN;
            goto finish;
        }
        if (model == STAT_MODEL_1VAR) {
            total += weight;
            weighted += weight * x;
            weightedSquares += weight * x * x;
            if (!isfinite((double)weighted) ||
                !isfinite((double)weightedSquares)) {
                error = CALC_ERR_RANGE;
                goto finish;
            }
        } else {
            error = addRegressionObservation(model, x, y, weight,
                                             &linear, &quadratic);
            if (error != CALC_OK)
                goto finish;
        }
    }
    if (model == STAT_MODEL_1VAR)
        error = finishOneVariable(runtime, total, weighted,
                                  weightedSquares);
    else if ((model == STAT_MODEL_QUADRATIC ? quadratic.weight :
              linear.weight) <= 0.0L)
        error = CALC_ERR_DOMAIN;
    else
        error = finishRegression(runtime, model, &linear, &quadratic);

finish:
    runtime->error = error;
    if (error != CALC_OK)
        modeSetResult(runtime, calcErrorText(error), error);
    return error;
}

u8 modeStatEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[24];
    char arguments[256];
    StatModel model;

    if (modeParseCommand(expression, name, sizeof(name), arguments,
                         sizeof(arguments))) {
        if (modeEquals(name, "freq") || modeEquals(name, "statfreq")) {
            modeSetStatModel(runtime, STAT_MODEL_1VAR);
            return statFrequency(runtime, arguments);
        }
        if (modeEquals(name, "stat1var") || modeEquals(name, "onevar")) {
            modeSetStatModel(runtime, STAT_MODEL_1VAR);
            return statBasic(runtime, arguments, "stat1var");
        }
        if (modelForCommand(name, &model)) {
            modeSetStatModel(runtime, model);
            return statRegression(runtime, arguments, model);
        }
        /* Descriptive statistics use the same canonical call convention. */
        if (modeEquals(name, "sum") || modeEquals(name, "prod") ||
            modeEquals(name, "mean") || modeEquals(name, "sumx2") ||
            modeEquals(name, "min") || modeEquals(name, "max") ||
            modeEquals(name, "median") || modeEquals(name, "varp") ||
            modeEquals(name, "vars") || modeEquals(name, "varsamp") ||
            modeEquals(name, "sdpop") || modeEquals(name, "sdsamp") ||
            modeEquals(name, "cuml"))
            return statBasic(runtime, arguments, name);
        return CALC_ERR_SYNTAX;
    }
    model = modeGetStatModel(runtime);
    /* Preserve the original compact paired-list contract for old programs.
       Dedicated 1-VAR frequency tables serialize through statfreq(). */
    if (model == STAT_MODEL_1VAR && strchr(expression, ',') != 0)
        model = STAT_MODEL_LINEAR;
    if (model == STAT_MODEL_1VAR)
        return statBasic(runtime, expression, 0);
    return statRegression(runtime, expression, model);
}
