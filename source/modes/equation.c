#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

#define EQUATION_MAX_DEGREE 6
#define EQUATION_MAX_ROOTS 6

static u8 makeDifference(const char *equation, char *output, u16 capacity)
{
    const char *equals = strchr(equation, '=');
    u16 leftLength;

    if (equals == 0 || strchr(equals + 1, '=') != 0)
        return 0;
    leftLength = (u16)(equals - equation);
    if (leftLength == 0 || equals[1] == '\0')
        return 0;
    return snprintf(output, capacity, "(%.*s)-(%s)",
                    (int)leftLength, equation, equals + 1) < (int)capacity;
}

static u8 evaluateX(ModeRuntime *runtime, const char *expression,
                    long double x, long double *value)
{
    CalcNumber saved;
    CalcNumber number;
    u8 hadSaved = calcContextGetVariable(&runtime->calc, 'X', &saved);
    u8 error;

    number = calcNumberFromLongDouble(x, &error);
    if (error != CALC_OK)
        return error;
    calcContextSetVariable(&runtime->calc, 'X', number);
    error = modeEvalReal(runtime, expression, value);
    if (hadSaved)
        calcContextSetVariable(&runtime->calc, 'X', saved);
    else
        runtime->calc.variableMask &= ~(1UL << ('X' - 'A'));
    return error;
}

static u8 newtonSolve(ModeRuntime *runtime, const char *difference,
                      long double seed, long double *root)
{
    long double x = seed;
    u8 iteration;

    for (iteration = 0; iteration < 40; iteration++) {
        long double value;
        long double plus;
        long double minus;
        long double step = (fabsl(x) + 1.0L) * 1e-5L;
        long double derivative;
        u8 error = evaluateX(runtime, difference, x, &value);
        if (error != CALC_OK)
            return error;
        if (value == 0.0L) {
            *root = x;
            return CALC_OK;
        }
        error = evaluateX(runtime, difference, x + step, &plus);
        if (error == CALC_OK)
            error = evaluateX(runtime, difference, x - step, &minus);
        if (error != CALC_OK)
            return error;
        derivative = (plus - minus) / (2.0L * step);
        if (fabsl(derivative) < 1e-14L)
            x += step * 10.0L;
        else {
            long double next = x - value / derivative;
            if (!isfinite((double)next) || fabsl(next) > 1e12L)
                return CALC_ERR_RANGE;
            /* CalcNumber retains nine significant digits.  Once an iterate
               moves by less than half that resolution, evaluating the same
               x can oscillate between adjacent mantissas even though Newton
               has converged (sqrt(2) is a representative case). */
            if (fabsl(next - x) < 5e-9L * (1.0L + fabsl(x))) {
                *root = next;
                return CALC_OK;
            }
            x = next;
        }
    }
    return CALC_ERR_DOMAIN;
}

static u8 bisectRoot(ModeRuntime *runtime, const char *difference,
                     long double left, long double right, long double *root)
{
    long double leftValue;
    long double rightValue;
    u8 iteration;
    u8 error = evaluateX(runtime, difference, left, &leftValue);
    if (error == CALC_OK)
        error = evaluateX(runtime, difference, right, &rightValue);
    if (error != CALC_OK)
        return error;
    if (leftValue == 0.0L) {
        *root = left;
        return CALC_OK;
    }
    if (rightValue == 0.0L) {
        *root = right;
        return CALC_OK;
    }
    if ((leftValue < 0.0L) == (rightValue < 0.0L))
        return CALC_ERR_DOMAIN;
    for (iteration = 0; iteration < 80; iteration++) {
        long double middle = (left + right) / 2.0L;
        long double middleValue;
        error = evaluateX(runtime, difference, middle, &middleValue);
        if (error != CALC_OK)
            return error;
        if (fabsl(middleValue) < 1e-12L ||
            fabsl(right - left) < 1e-11L * (1.0L + fabsl(middle))) {
            *root = middle;
            return CALC_OK;
        }
        if ((leftValue < 0.0L) != (middleValue < 0.0L)) {
            right = middle;
            rightValue = middleValue;
        } else {
            left = middle;
            leftValue = middleValue;
        }
    }
    *root = (left + right) / 2.0L;
    return CALC_OK;
}

static void sortRoots(long double *roots, u8 count);

static u8 executeSolve(ModeRuntime *runtime, const char *body,
                       u8 multiple)
{
    char fields[3][64];
    char difference[140];
    u8 count;
    u8 error;
    char output[MODE_RESULT_CAPACITY];

    if (!modeSplit(body, ';', fields, multiple ? 3 : 2, &count) ||
        count != (multiple ? 3 : 2) ||
        !makeDifference(fields[0], difference, sizeof(difference)))
        return CALC_ERR_SYNTAX;
    if (!multiple) {
        long double seed;
        long double root;
        error = modeEvalReal(runtime, fields[1], &seed);
        if (error == CALC_OK)
            error = newtonSolve(runtime, difference, seed, &root);
        if (error != CALC_OK)
            return error;
        modeFormatReal(root, output, sizeof(output), 10);
    } else {
        long double minimum;
        long double maximum;
        long double previousX;
        long double previousValue;
        long double olderValue = 0.0L;
        long double roots[16];
        u8 rootCount = 0;
        u8 previousValid;
        u8 olderValid = 0;
        u16 sample;
        u16 used = 0;

        error = modeEvalReal(runtime, fields[1], &minimum);
        if (error == CALC_OK)
            error = modeEvalReal(runtime, fields[2], &maximum);
        if (error != CALC_OK || minimum >= maximum)
            return error == CALC_OK ? CALC_ERR_DOMAIN : error;
        previousX = minimum;
        error = evaluateX(runtime, difference, previousX, &previousValue);
        previousValid = error == CALC_OK;
        if (previousValid && previousValue == 0.0L)
            roots[rootCount++] = previousX;
        for (sample = 1; sample <= 512; sample++) {
            long double x = minimum + (maximum - minimum) * sample / 512.0L;
            long double value = 0.0L;
            error = evaluateX(runtime, difference, x, &value);
            if (error == CALC_OK && value == 0.0L &&
                rootCount < (u8)(sizeof(roots) / sizeof(roots[0]))) {
                u8 duplicate = 0;
                u8 rootIndex;
                for (rootIndex = 0; rootIndex < rootCount; rootIndex++)
                    if (fabsl(x - roots[rootIndex]) <= 1e-6L)
                        duplicate = 1;
                if (!duplicate)
                    roots[rootCount++] = x;
            }
            if (error == CALC_OK && previousValid &&
                (previousValue < 0.0L) != (value < 0.0L)) {
                long double root;
                if (bisectRoot(runtime, difference, previousX, x, &root) ==
                    CALC_OK) {
                    long double rootValue;
                    if (evaluateX(runtime, difference, root, &rootValue) ==
                            CALC_OK &&
                        fabsl(rootValue) <= 1e-5L *
                            (fabsl(previousValue) + fabsl(value)) &&
                        rootCount < (u8)(sizeof(roots) / sizeof(roots[0]))) {
                        u8 duplicate = 0;
                        u8 rootIndex;
                        for (rootIndex = 0; rootIndex < rootCount;
                             rootIndex++)
                            if (fabsl(root - roots[rootIndex]) <= 1e-6L)
                                duplicate = 1;
                        if (!duplicate)
                            roots[rootCount++] = root;
                    }
                }
            }
            /* A sign scan misses even-multiplicity roots.  A local minimum of
               |f| is a cheap, bounded place to try Newton; the returned root
               is accepted only inside the requested interval and if it is
               distinct from roots already found. */
            if (error == CALC_OK && previousValid && olderValid &&
                fabsl(previousValue) <= fabsl(olderValue) &&
                fabsl(previousValue) <= fabsl(value)) {
                long double root;
                if (newtonSolve(runtime, difference, previousX, &root) ==
                        CALC_OK && root >= minimum && root <= maximum &&
                    rootCount < (u8)(sizeof(roots) / sizeof(roots[0]))) {
                    long double rootValue;
                    if (evaluateX(runtime, difference, root, &rootValue) ==
                            CALC_OK &&
                        fabsl(rootValue) <= 1e-5L *
                            (fabsl(olderValue) + fabsl(value))) {
                        u8 duplicate = 0;
                        u8 rootIndex;
                        for (rootIndex = 0; rootIndex < rootCount;
                             rootIndex++)
                            if (fabsl(root - roots[rootIndex]) <= 1e-6L)
                                duplicate = 1;
                        if (!duplicate)
                            roots[rootCount++] = root;
                    }
                }
            }
            if (error == CALC_OK) {
                olderValue = previousValue;
                olderValid = previousValid;
                previousX = x;
                previousValue = value;
                previousValid = 1;
            } else {
                previousValid = 0;
                olderValid = 0;
            }
        }
        if (rootCount == 0)
            return CALC_ERR_DOMAIN;
        sortRoots(roots, rootCount);
        output[0] = '\0';
        for (sample = 0; sample < rootCount; sample++) {
            char value[28];
            int written;
            modeFormatReal(roots[sample], value, sizeof(value), 9);
            written = snprintf(output + used, sizeof(output) - used,
                               "%sX%u=%s", sample == 0 ? "" : " ",
                               sample + 1, value);
            if (written < 0 || written >= (int)(sizeof(output) - used))
                break;
            used = (u16)(used + written);
        }
    }
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static long double polynomialValue(const long double *coefficient,
                                   u8 degree, long double x)
{
    long double result = coefficient[0];
    u8 index;
    for (index = 1; index <= degree; index++)
        result = result * x + coefficient[index];
    return result;
}

static void addRoot(long double *roots, u8 *count, long double root)
{
    u8 index;
    for (index = 0; index < *count; index++)
        if (fabsl(roots[index] - root) < 1e-7L)
            return;
    if (*count < EQUATION_MAX_ROOTS)
        roots[(*count)++] = root;
}

static void sortRoots(long double *roots, u8 count)
{
    u8 index;
    for (index = 1; index < count; index++) {
        long double root = roots[index];
        s16 position = (s16)index - 1;
        while (position >= 0 && roots[position] > root) {
            roots[position + 1] = roots[position];
            position--;
        }
        roots[position + 1] = root;
    }
}

static u8 mergeClosePolynomialRoots(const long double *coefficient,
                                    u8 degree, long double *roots, u8 count)
{
    u8 read;
    u8 write = 0;

    for (read = 0; read < count; read++) {
        if (write != 0 &&
            fabsl(roots[read] - roots[write - 1]) <=
                1e-5L * (1.0L + fabsl(roots[read]))) {
            if (fabsl(polynomialValue(coefficient, degree, roots[read])) <
                fabsl(polynomialValue(coefficient, degree,
                                      roots[write - 1])))
                roots[write - 1] = roots[read];
        } else
            roots[write++] = roots[read];
    }
    return write;
}

static u8 polynomialRealRoots(const long double *coefficient, u8 degree,
                              long double *roots)
{
    long double derivative[EQUATION_MAX_DEGREE];
    long double critical[EQUATION_MAX_ROOTS];
    long double points[EQUATION_MAX_ROOTS + 2];
    long double bound = 1.0L;
    u8 criticalCount = 0;
    u8 pointCount;
    u8 count = 0;
    u8 index;

    if (degree == 0)
        return 0;
    if (degree == 1) {
        if (coefficient[0] != 0.0L) {
            roots[0] = -coefficient[1] / coefficient[0];
            return 1;
        }
        return 0;
    }
    for (index = 1; index <= degree; index++) {
        long double ratio = fabsl(coefficient[index] / coefficient[0]);
        if (1.0L + ratio > bound)
            bound = 1.0L + ratio;
    }
    for (index = 0; index < degree; index++)
        derivative[index] = coefficient[index] * (degree - index);
    criticalCount = polynomialRealRoots(derivative, (u8)(degree - 1),
                                        critical);
    sortRoots(critical, criticalCount);
    points[0] = -bound;
    for (index = 0; index < criticalCount; index++)
        points[index + 1] = critical[index];
    points[criticalCount + 1] = bound;
    pointCount = (u8)(criticalCount + 2);
    for (index = 0; index < criticalCount; index++) {
        long double value = polynomialValue(coefficient, degree,
                                            critical[index]);
        if (fabsl(value) < 1e-9L)
            addRoot(roots, &count, critical[index]);
    }
    for (index = 0; index + 1 < pointCount; index++) {
        long double left = points[index];
        long double right = points[index + 1];
        long double leftValue = polynomialValue(coefficient, degree, left);
        long double rightValue = polynomialValue(coefficient, degree, right);
        u8 iteration;
        if ((leftValue < 0.0L) == (rightValue < 0.0L))
            continue;
        for (iteration = 0; iteration < 90; iteration++) {
            long double middle = (left + right) / 2.0L;
            long double value = polynomialValue(coefficient, degree, middle);
            if (fabsl(value) < 1e-13L) {
                left = right = middle;
                break;
            }
            if ((leftValue < 0.0L) != (value < 0.0L))
                right = middle;
            else {
                left = middle;
                leftValue = value;
            }
        }
        addRoot(roots, &count, (left + right) / 2.0L);
    }
    sortRoots(roots, count);
    return mergeClosePolynomialRoots(coefficient, degree, roots, count);
}

static u8 executePolynomial(ModeRuntime *runtime, const char *body)
{
    long double coefficient[EQUATION_MAX_DEGREE + 1];
    long double roots[EQUATION_MAX_ROOTS];
    u8 coefficientCount;
    u8 degree;
    u8 rootCount;
    u8 index;
    u8 error;
    u16 used = 0;
    char output[MODE_RESULT_CAPACITY];

    error = modeParseRealList(runtime, body, ';', coefficient,
                              EQUATION_MAX_DEGREE + 1, &coefficientCount);
    if (error != CALC_OK || coefficientCount < 3)
        return error == CALC_OK ? CALC_ERR_SYNTAX : error;
    degree = (u8)(coefficientCount - 1);
    if (fabsl(coefficient[0]) < 1e-18L)
        return CALC_ERR_DOMAIN;
    if (degree == 2) {
        long double discriminant = coefficient[1] * coefficient[1] -
            4.0L * coefficient[0] * coefficient[2];
        if (discriminant < 0.0L)
            return CALC_ERR_DOMAIN;
        roots[0] = (-coefficient[1] + sqrtl(discriminant)) /
                   (2.0L * coefficient[0]);
        roots[1] = (-coefficient[1] - sqrtl(discriminant)) /
                   (2.0L * coefficient[0]);
        rootCount = fabsl(roots[0] - roots[1]) < 1e-8L ? 1 : 2;
    } else
        rootCount = polynomialRealRoots(coefficient, degree, roots);
    if (rootCount == 0)
        return CALC_ERR_DOMAIN;
    output[0] = '\0';
    for (index = 0; index < rootCount; index++) {
        char root[28];
        int written;
        modeFormatReal(roots[index], root, sizeof(root), 9);
        written = snprintf(output + used, sizeof(output) - used,
                           "%sX%u=%s", index == 0 ? "" : " ",
                           index + 1, root);
        if (written < 0 || written >= (int)(sizeof(output) - used))
            break;
        used = (u16)(used + written);
    }
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static u8 executeLinearSystem(ModeRuntime *runtime, const char *body)
{
    char rows[4][64];
    long double matrix[4][5];
    long double solution[4];
    u8 rowCount;
    u8 row;
    u8 column;
    u16 used = 0;
    char output[MODE_RESULT_CAPACITY];

    memset(matrix, 0, sizeof(matrix));
    if (!modeSplit(body, ';', rows, 4, &rowCount) || rowCount < 2)
        return CALC_ERR_SYNTAX;
    for (row = 0; row < rowCount; row++) {
        u8 columns;
        u8 error = modeParseRealList(runtime, rows[row], ',', matrix[row],
                                     5, &columns);
        if (error != CALC_OK)
            return error;
        if (columns != rowCount + 1)
            return CALC_ERR_SYNTAX;
    }
    for (column = 0; column < rowCount; column++) {
        u8 best = column;
        long double divisor;
        for (row = (u8)(column + 1); row < rowCount; row++)
            if (fabsl(matrix[row][column]) > fabsl(matrix[best][column]))
                best = row;
        if (fabsl(matrix[best][column]) < 1e-15L)
            return CALC_ERR_DOMAIN;
        if (best != column) {
            u8 item;
            for (item = column; item <= rowCount; item++) {
                long double swap = matrix[column][item];
                matrix[column][item] = matrix[best][item];
                matrix[best][item] = swap;
            }
        }
        divisor = matrix[column][column];
        for (row = (u8)(column + 1); row < rowCount; row++) {
            long double factor = matrix[row][column] / divisor;
            u8 item;
            for (item = column; item <= rowCount; item++)
                matrix[row][item] -= factor * matrix[column][item];
        }
    }
    for (row = rowCount; row-- > 0;) {
        long double value = matrix[row][rowCount];
        for (column = (u8)(row + 1); column < rowCount; column++)
            value -= matrix[row][column] * solution[column];
        solution[row] = value / matrix[row][row];
    }
    output[0] = '\0';
    for (row = 0; row < rowCount; row++) {
        char value[28];
        int written;
        modeFormatReal(solution[row], value, sizeof(value), 9);
        written = snprintf(output + used, sizeof(output) - used,
                           "%s%c=%s", row == 0 ? "" : " ",
                           'X' + row, value);
        if (written < 0 || written >= (int)(sizeof(output) - used))
            break;
        used = (u16)(used + written);
    }
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

u8 modeEquationEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[16];
    char arguments[256];

    if (modeParseCommand(expression, name, sizeof(name), arguments,
                         sizeof(arguments))) {
        if (modeEquals(name, "lin") || modeEquals(name, "linear"))
            return executeLinearSystem(runtime, arguments);
        if (modeEquals(name, "poly") || modeEquals(name, "polynomial"))
            return executePolynomial(runtime, arguments);
        if (modeEquals(name, "solve")) {
            char fields[2][64];
            u8 count;
            if (!modeSplit(arguments, ';', fields, 2, &count))
                return CALC_ERR_SYNTAX;
            if (count == 1) {
                char wrapped[180];
                if (snprintf(wrapped, sizeof(wrapped), "%s;0", arguments) >=
                    (int)sizeof(wrapped))
                    return CALC_ERR_RANGE;
                return executeSolve(runtime, wrapped, 0);
            }
            return executeSolve(runtime, arguments, 0);
        }
        if (modeEquals(name, "solven"))
            return executeSolve(runtime, arguments, 1);
        return CALC_ERR_SYNTAX;
    }
    {
        char wrapped[150];
        if (snprintf(wrapped, sizeof(wrapped), "%s;0", expression) >=
                (int)sizeof(wrapped))
            return CALC_ERR_SYNTAX;
        /* Default solve syntax uses zero as the initial seed. */
        return executeSolve(runtime, wrapped, 0);
    }
}
