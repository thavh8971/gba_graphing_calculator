#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

#define INEQ_MAX_DEGREE 4
#define INEQ_MAX_ROOTS 4

typedef struct RelationParts {
    char left[64];
    char right[64];
    char relation[3];
} RelationParts;

static u8 validRelation(const char *relation)
{
    return modeEquals(relation, "<") || modeEquals(relation, "<=") ||
           modeEquals(relation, ">") || modeEquals(relation, ">=");
}

static u8 splitRelation(const char *text, RelationParts *parts)
{
    u16 index;
    s16 depth = 0;
    u8 found = 0;

    memset(parts, 0, sizeof(*parts));
    for (index = 0; text[index] != '\0'; index++) {
        u8 length = 1;
        if (text[index] == '(' || text[index] == '[') {
            depth++;
            continue;
        }
        if (text[index] == ')' || text[index] == ']') {
            if (depth == 0)
                return 0;
            depth--;
            continue;
        }
        if (depth != 0 || (text[index] != '<' && text[index] != '>'))
            continue;
        if (found)
            return 0;
        if (text[index + 1] == '=')
            length = 2;
        if (index == 0 || index >= sizeof(parts->left) ||
            strlen(text + index + length) >= sizeof(parts->right))
            return 0;
        memcpy(parts->left, text, index);
        parts->left[index] = '\0';
        parts->relation[0] = text[index];
        if (length == 2)
            parts->relation[1] = '=';
        strcpy(parts->right, text + index + length);
        found = 1;
        index = (u16)(index + length - 1);
    }
    return found && depth == 0 && parts->right[0] != '\0';
}

static u8 evaluateAtX(ModeRuntime *runtime, const char *expression,
                      long double x, long double *value)
{
    CalcNumber saved;
    CalcNumber xNumber;
    u8 hadSaved = calcContextGetVariable(&runtime->calc, 'X', &saved);
    u8 error;

    xNumber = calcNumberFromLongDouble(x, &error);
    if (error != CALC_OK)
        return error;
    calcContextSetVariable(&runtime->calc, 'X', xNumber);
    error = modeEvalReal(runtime, expression, value);
    if (hadSaved)
        calcContextSetVariable(&runtime->calc, 'X', saved);
    else
        runtime->calc.variableMask &= ~(1UL << ('X' - 'A'));
    return error;
}

static u8 relationTrue(long double value, const char *relation)
{
    if (relation[0] == '<')
        return relation[1] == '=' ? value <= 0.0L : value < 0.0L;
    return relation[1] == '=' ? value >= 0.0L : value > 0.0L;
}

static const char *flippedRelation(const char *relation)
{
    if (strcmp(relation, "<") == 0)
        return ">";
    if (strcmp(relation, "<=") == 0)
        return ">=";
    if (strcmp(relation, ">") == 0)
        return "<";
    return "<=";
}

static u8 solveLinear(ModeRuntime *runtime, const RelationParts *parts)
{
    long double left0;
    long double left1;
    long double right0;
    long double right1;
    long double coefficient;
    long double constant;
    long double boundary;
    const char *relation = parts->relation;
    char value[32];
    char output[64];
    u8 error;

    error = evaluateAtX(runtime, parts->left, 0.0L, &left0);
    if (error == CALC_OK)
        error = evaluateAtX(runtime, parts->left, 1.0L, &left1);
    if (error == CALC_OK)
        error = evaluateAtX(runtime, parts->right, 0.0L, &right0);
    if (error == CALC_OK)
        error = evaluateAtX(runtime, parts->right, 1.0L, &right1);
    if (error != CALC_OK)
        return error;
    coefficient = (left1 - left0) - (right1 - right0);
    constant = left0 - right0;
    if (fabsl(coefficient) < 1e-12L) {
        modeSetResult(runtime, relationTrue(constant, relation) ?
                      "ALL REAL X" : "NO SOLUTION", CALC_OK);
        return CALC_OK;
    }
    boundary = -constant / coefficient;
    if (coefficient < 0.0L)
        relation = flippedRelation(relation);
    modeFormatReal(boundary, value, sizeof(value), 9);
    snprintf(output, sizeof(output), "X%s%s", relation, value);
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}

static long double polynomialValue(const long double *coefficient,
                                   u8 degree, long double x)
{
    long double value = coefficient[0];
    u8 index;

    for (index = 1; index <= degree; index++)
        value = value * x + coefficient[index];
    return value;
}

static long double polynomialMagnitude(const long double *coefficient,
                                       u8 degree, long double x)
{
    long double value = fabsl(coefficient[0]);
    u8 index;

    x = fabsl(x);
    for (index = 1; index <= degree; index++)
        value = value * x + fabsl(coefficient[index]);
    return value;
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

static void addRoot(long double *roots, u8 *count, long double root)
{
    u8 index;

    if (!isfinite((double)root))
        return;
    for (index = 0; index < *count; index++)
        if (fabsl(roots[index] - root) <=
            1e-8L * (1.0L + fabsl(root)))
            return;
    if (*count < INEQ_MAX_ROOTS)
        roots[(*count)++] = root;
}

/* Derivative roots split a degree <= 4 polynomial into monotone intervals.
   Sign changes isolate odd-multiplicity roots; a near-zero critical value
   catches even-multiplicity roots without a dense scan. */
static u8 polynomialRoots(const long double *coefficient, u8 degree,
                          long double *roots, u8 *rootCount)
{
    long double derivative[INEQ_MAX_DEGREE];
    long double critical[INEQ_MAX_ROOTS];
    long double points[INEQ_MAX_ROOTS + 2];
    long double bound = 1.0L;
    u8 criticalCount = 0;
    u8 count = 0;
    u8 index;
    u8 error;

    *rootCount = 0;
    if (degree == 0)
        return CALC_OK;
    if (degree == 1) {
        long double root;
        if (coefficient[0] == 0.0L)
            return CALC_OK;
        root = -coefficient[1] / coefficient[0];
        if (!isfinite((double)root))
            return CALC_ERR_RANGE;
        roots[0] = root;
        *rootCount = 1;
        return CALC_OK;
    }
    for (index = 1; index <= degree; index++) {
        long double candidate = 1.0L +
            fabsl(coefficient[index] / coefficient[0]);
        if (!isfinite((double)candidate))
            return CALC_ERR_RANGE;
        if (candidate > bound)
            bound = candidate;
    }
    for (index = 0; index < degree; index++)
        derivative[index] = coefficient[index] * (degree - index);
    error = polynomialRoots(derivative, (u8)(degree - 1), critical,
                            &criticalCount);
    if (error != CALC_OK)
        return error;
    sortRoots(critical, criticalCount);
    points[0] = -bound;
    for (index = 0; index < criticalCount; index++)
        points[index + 1] = critical[index];
    points[criticalCount + 1] = bound;

    for (index = 0; index < criticalCount; index++) {
        long double x = critical[index];
        long double value = polynomialValue(coefficient, degree, x);
        long double tolerance = 1e-9L *
            (1.0L + polynomialMagnitude(coefficient, degree, x));
        if (fabsl(value) <= tolerance)
            addRoot(roots, &count, x);
    }
    for (index = 0; index <= criticalCount; index++) {
        long double left = points[index];
        long double right = points[index + 1];
        long double leftValue = polynomialValue(coefficient, degree, left);
        long double rightValue = polynomialValue(coefficient, degree, right);
        u8 iteration;

        if (leftValue == 0.0L)
            addRoot(roots, &count, left);
        if (rightValue == 0.0L)
            addRoot(roots, &count, right);
        if ((leftValue < 0.0L) == (rightValue < 0.0L) ||
            leftValue == 0.0L || rightValue == 0.0L)
            continue;
        for (iteration = 0; iteration < 96; iteration++) {
            long double middle = (left + right) / 2.0L;
            long double value = polynomialValue(coefficient, degree, middle);
            if (value == 0.0L) {
                left = right = middle;
                break;
            }
            if ((leftValue < 0.0L) != (value < 0.0L)) {
                right = middle;
                rightValue = value;
            } else {
                left = middle;
                leftValue = value;
            }
        }
        addRoot(roots, &count, (left + right) / 2.0L);
    }
    sortRoots(roots, count);
    *rootCount = count;
    return CALC_OK;
}

static u8 interpolatePolynomial(ModeRuntime *runtime,
                                const RelationParts *parts, u8 degree,
                                long double *coefficient)
{
    static const long double samples[3][5] = {
        {-1.0L, 0.0L, 1.0L, 0.0L, 0.0L},
        {-1.0L, 0.0L, 1.0L, 2.0L, 0.0L},
        {-2.0L, -1.0L, 0.0L, 1.0L, 2.0L}
    };
    long double matrix[INEQ_MAX_DEGREE + 1][INEQ_MAX_DEGREE + 2];
    u8 size = (u8)(degree + 1);
    u8 row;
    u8 column;

    memset(matrix, 0, sizeof(matrix));
    for (row = 0; row < size; row++) {
        long double x = samples[degree - 2][row];
        long double left;
        long double right;
        long double power = 1.0L;
        u8 error = evaluateAtX(runtime, parts->left, x, &left);
        if (error == CALC_OK)
            error = evaluateAtX(runtime, parts->right, x, &right);
        if (error != CALC_OK)
            return error;
        for (column = size; column-- > 0;) {
            matrix[row][column] = power;
            power *= x;
        }
        matrix[row][size] = left - right;
    }
    for (column = 0; column < size; column++) {
        u8 best = column;
        long double divisor;
        for (row = (u8)(column + 1); row < size; row++)
            if (fabsl(matrix[row][column]) >
                fabsl(matrix[best][column]))
                best = row;
        if (fabsl(matrix[best][column]) < 1e-18L)
            return CALC_ERR_DOMAIN;
        if (best != column) {
            u8 item;
            for (item = column; item <= size; item++) {
                long double swap = matrix[column][item];
                matrix[column][item] = matrix[best][item];
                matrix[best][item] = swap;
            }
        }
        divisor = matrix[column][column];
        for (row = (u8)(column + 1); row < size; row++) {
            long double factor = matrix[row][column] / divisor;
            u8 item;
            for (item = column; item <= size; item++)
                matrix[row][item] -= factor * matrix[column][item];
        }
    }
    for (row = size; row-- > 0;) {
        long double value = matrix[row][size];
        for (column = (u8)(row + 1); column < size; column++)
            value -= matrix[row][column] * coefficient[column];
        coefficient[row] = value / matrix[row][row];
    }
    return CALC_OK;
}

static u8 parseCoefficientCall(ModeRuntime *runtime, const char *body,
                               u8 degree, long double *coefficient,
                               char relation[3])
{
    char fields[INEQ_MAX_DEGREE + 2][64];
    u8 count;
    u8 index;

    if (!modeSplit(body, ';', fields, (u8)(degree + 2), &count) ||
        count != degree + 2 || !validRelation(fields[degree + 1]))
        return CALC_ERR_SYNTAX;
    for (index = 0; index <= degree; index++) {
        u8 error = modeEvalReal(runtime, fields[index], &coefficient[index]);
        if (error != CALC_OK)
            return error;
    }
    strcpy(relation, fields[degree + 1]);
    return CALC_OK;
}

static u8 appendSolution(char *output, u16 capacity, u16 *used,
                         const char *text)
{
    u16 length = (u16)strlen(text);

    if (*used + length + 1 > capacity)
        return 0;
    memcpy(output + *used, text, length + 1);
    *used = (u16)(*used + length);
    return 1;
}

static u8 formatPolynomialSolution(ModeRuntime *runtime,
                                   const long double *coefficient,
                                   u8 degree, const char *relation,
                                   const long double *roots, u8 rootCount)
{
    u8 truth[INEQ_MAX_ROOTS * 2 + 1];
    u8 nodeCount = (u8)(rootCount * 2 + 1);
    u8 closed = relation[1] == '=';
    u8 node;
    u8 any = 0;
    u16 used = 0;
    char output[MODE_RESULT_CAPACITY];

    if (rootCount == 0) {
        modeSetResult(runtime,
                      relationTrue(polynomialValue(coefficient, degree, 0.0L),
                                   relation) ? "ALL REAL X" : "NO SOLUTION",
                      CALC_OK);
        return CALC_OK;
    }
    truth[0] = relationTrue((degree & 1) ? -coefficient[0] :
                            coefficient[0], relation);
    for (node = 0; node < rootCount; node++) {
        truth[node * 2 + 1] = closed;
        if (node + 1 < rootCount) {
            long double middle = (roots[node] + roots[node + 1]) / 2.0L;
            truth[node * 2 + 2] = relationTrue(
                polynomialValue(coefficient, degree, middle), relation);
        }
    }
    truth[nodeCount - 1] = relationTrue(coefficient[0], relation);
    output[0] = '\0';
    node = 0;
    while (node < nodeCount) {
        u8 start;
        u8 end;
        s8 lowerIndex = -1;
        s8 upperIndex = -1;
        u8 lowerClosed = 0;
        u8 upperClosed = 0;
        char lower[32];
        char upper[32];
        char piece[80];

        while (node < nodeCount && !truth[node])
            node++;
        if (node == nodeCount)
            break;
        start = node;
        while (node + 1 < nodeCount && truth[node + 1])
            node++;
        end = node++;
        if (start != 0) {
            if (start & 1) {
                lowerIndex = (s8)(start / 2);
                lowerClosed = 1;
            } else
                lowerIndex = (s8)(start / 2 - 1);
        }
        if (end + 1 != nodeCount) {
            if (end & 1) {
                upperIndex = (s8)(end / 2);
                upperClosed = 1;
            } else
                upperIndex = (s8)(end / 2);
        }
        if (any && !appendSolution(output, sizeof(output), &used, " OR "))
            return CALC_ERR_RANGE;
        any = 1;
        if (lowerIndex < 0 && upperIndex < 0) {
            modeSetResult(runtime, "ALL REAL X", CALC_OK);
            return CALC_OK;
        }
        if (lowerIndex >= 0)
            modeFormatReal(roots[(u8)lowerIndex], lower, sizeof(lower), 9);
        if (upperIndex >= 0)
            modeFormatReal(roots[(u8)upperIndex], upper, sizeof(upper), 9);
        if (lowerIndex >= 0 && upperIndex >= 0 &&
            lowerIndex == upperIndex && lowerClosed && upperClosed)
            snprintf(piece, sizeof(piece), "X=%s", lower);
        else if (lowerIndex < 0)
            snprintf(piece, sizeof(piece), "X%s%s",
                     upperClosed ? "<=" : "<", upper);
        else if (upperIndex < 0)
            snprintf(piece, sizeof(piece), "X%s%s",
                     lowerClosed ? ">=" : ">", lower);
        else
            snprintf(piece, sizeof(piece), "%s%sX%s%s", lower,
                     lowerClosed ? "<=" : "<",
                     upperClosed ? "<=" : "<", upper);
        if (!appendSolution(output, sizeof(output), &used, piece))
            return CALC_ERR_RANGE;
    }
    modeSetResult(runtime, any ? output : "NO SOLUTION", CALC_OK);
    return CALC_OK;
}

static u8 solvePolynomial(ModeRuntime *runtime, long double *coefficient,
                          u8 degree, const char *relation)
{
    long double roots[INEQ_MAX_ROOTS];
    u8 rootCount;
    u8 error;

    while (degree != 0 && coefficient[0] == 0.0L) {
        u8 index;
        for (index = 0; index < degree; index++)
            coefficient[index] = coefficient[index + 1];
        degree--;
    }
    if (degree == 0) {
        modeSetResult(runtime, relationTrue(coefficient[0], relation) ?
                      "ALL REAL X" : "NO SOLUTION", CALC_OK);
        return CALC_OK;
    }
    error = polynomialRoots(coefficient, degree, roots, &rootCount);
    if (error != CALC_OK)
        return error;
    return formatPolynomialSolution(runtime, coefficient, degree, relation,
                                    roots, rootCount);
}

static u8 solveRelationPolynomial(ModeRuntime *runtime, const char *body,
                                  u8 degree)
{
    RelationParts parts;
    long double coefficient[INEQ_MAX_DEGREE + 1] = {
        0.0L, 0.0L, 0.0L, 0.0L, 0.0L
    };
    u8 error;

    if (!splitRelation(body, &parts))
        return CALC_ERR_SYNTAX;
    error = interpolatePolynomial(runtime, &parts, degree, coefficient);
    if (error != CALC_OK)
        return error;
    return solvePolynomial(runtime, coefficient, degree, parts.relation);
}

u8 modeInequalityEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[20];
    char arguments[256];
    RelationParts parts;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));

    if (!command) {
        if (!splitRelation(expression, &parts))
            return CALC_ERR_SYNTAX;
        return solveLinear(runtime, &parts);
    }
    if (modeEquals(name, "linear") || modeEquals(name, "ineq")) {
        if (!splitRelation(arguments, &parts))
            return CALC_ERR_SYNTAX;
        return solveLinear(runtime, &parts);
    }
    if (modeEquals(name, "ineq2") || modeEquals(name, "ineq3") ||
        modeEquals(name, "ineq4")) {
        long double coefficient[INEQ_MAX_DEGREE + 1] = {
            0.0L, 0.0L, 0.0L, 0.0L, 0.0L
        };
        char relation[3];
        u8 degree = modeEquals(name, "ineq2") ? 2 :
                    modeEquals(name, "ineq3") ? 3 : 4;
        u8 error = parseCoefficientCall(runtime, arguments, degree,
                                        coefficient, relation);
        if (error != CALC_OK)
            return error;
        return solvePolynomial(runtime, coefficient, degree, relation);
    }
    if (modeEquals(name, "quad") || modeEquals(name, "quadratic") ||
        modeEquals(name, "cubic") || modeEquals(name, "quartic")) {
        u8 degree = (modeEquals(name, "quad") ||
                     modeEquals(name, "quadratic")) ? 2 :
                    modeEquals(name, "cubic") ? 3 : 4;
        char fields[INEQ_MAX_DEGREE + 2][64];
        u8 count;
        if (modeSplit(arguments, ';', fields, (u8)(degree + 2), &count) &&
            count == degree + 2) {
            long double coefficient[INEQ_MAX_DEGREE + 1] = {
                0.0L, 0.0L, 0.0L, 0.0L, 0.0L
            };
            char relation[3];
            u8 error = parseCoefficientCall(runtime, arguments, degree,
                                            coefficient, relation);
            if (error != CALC_OK)
                return error;
            return solvePolynomial(runtime, coefficient, degree, relation);
        }
        return solveRelationPolynomial(runtime, arguments, degree);
    }
    return CALC_ERR_SYNTAX;
}
