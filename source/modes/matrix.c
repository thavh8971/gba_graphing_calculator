#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

/* A UI cell stores at most 15 visible characters. Four cells plus three
   commas therefore fit exactly in a 63-character row. A bracketed 4x4
   operand needs 257 visible characters, and a binary call needs two such
   operands plus its top-level separator. Capacities below include NUL. */
#define MATRIX_ROW_TEXT_CAPACITY 64
#define MATRIX_TEXT_CAPACITY 258
#define MATRIX_BINARY_ARGUMENT_CAPACITY 516

typedef struct MatrixValue {
    long double cell[4][4];
    u8 rows;
    u8 columns;
} MatrixValue;

static u8 matrixRegisterIndex(const char *text, ModeNamedRegister *name)
{
    u8 index;

    for (index = 0; index < MODE_NAMED_REGISTER_COUNT; index++) {
        if (modeEquals(text,
                       modeMatrixRegisterLabel((ModeNamedRegister)index))) {
            *name = (ModeNamedRegister)index;
            return 1;
        }
    }
    return 0;
}

static u8 parseMatrix(ModeRuntime *runtime, const char *text,
                      MatrixValue *matrix)
{
    char rows[MODE_MATRIX_MAX_ROWS][MATRIX_ROW_TEXT_CAPACITY];
    char unwrapped[MATRIX_TEXT_CAPACITY];
    const char *start = text;
    u16 length;
    u8 rowCount;
    u8 row;

    memset(matrix, 0, sizeof(*matrix));
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
    text = unwrapped;
    {
        ModeNamedRegister name;
        if (matrixRegisterIndex(text, &name)) {
            ModeMatrixRegister stored;
            u8 row;
            u8 column;

            if (!modeMatrixGetRegister(runtime, name, &stored))
                return CALC_ERR_DOMAIN;
            matrix->rows = stored.rows;
            matrix->columns = stored.columns;
            for (row = 0; row < stored.rows; row++)
                for (column = 0; column < stored.columns; column++)
                    matrix->cell[row][column] = calcNumberToLongDouble(
                        stored.cell[row][column]);
            return CALC_OK;
        }
    }
    if (*text == '[') {
        if (length < 2 || text[length - 1] != ']')
            return CALC_ERR_SYNTAX;
        memmove(unwrapped, unwrapped + 1, length - 2);
        unwrapped[length - 2] = '\0';
    }
    if (!modeSplit(text, ';', rows, 4, &rowCount))
        return CALC_ERR_SYNTAX;
    for (row = 0; row < rowCount; row++) {
        char fields[MODE_MATRIX_MAX_COLUMNS][MATRIX_ROW_TEXT_CAPACITY];
        u8 columns;
        u8 column;

        /* modeParseRealList reserves MODE_MAX_VALUES fields (4 KiB) even
           though a matrix row can only contain four. Splitting locally keeps
           this hot parser's stack bounded while preserving identical token
           and whitespace rules. */
        if (!modeSplit(rows[row], ',', fields, MODE_MATRIX_MAX_COLUMNS,
                       &columns))
            return CALC_ERR_SYNTAX;
        if (row == 0)
            matrix->columns = columns;
        else if (columns != matrix->columns)
            return CALC_ERR_SYNTAX;
        for (column = 0; column < columns; column++) {
            u8 error = modeEvalReal(runtime, fields[column],
                                    &matrix->cell[row][column]);
            if (error != CALC_OK)
                return error;
        }
    }
    matrix->rows = rowCount;
    return CALC_OK;
}

u8 modeMatrixSetRegisterExpression(ModeRuntime *runtime,
                                   ModeNamedRegister name,
                                   const char *expression)
{
    MatrixValue parsed;
    CalcNumber cells[MODE_MATRIX_MAX_ROWS][MODE_MATRIX_MAX_COLUMNS];
    u8 row;
    u8 column;
    u8 error;

    if (runtime == 0 || expression == 0 ||
        (u8)name >= MODE_NAMED_REGISTER_COUNT)
        return CALC_ERR_SYNTAX;
    error = parseMatrix(runtime, expression, &parsed);
    if (error != CALC_OK)
        return error;
    memset(cells, 0, sizeof(cells));
    for (row = 0; row < parsed.rows; row++) {
        for (column = 0; column < parsed.columns; column++) {
            cells[row][column] = calcNumberFromLongDouble(
                parsed.cell[row][column], &error);
            if (error != CALC_OK)
                return error;
        }
    }
    return modeMatrixSetRegister(runtime, name, cells, parsed.rows,
                                 parsed.columns) ? CALC_OK :
                                                   CALC_ERR_SYNTAX;
}

static u8 splitMatrixPair(const char *text,
                          char left[MATRIX_TEXT_CAPACITY],
                          char right[MATRIX_TEXT_CAPACITY])
{
    u16 index;
    u16 separator = 0;
    s16 depth = 0;
    u8 pipes = 0;
    u8 semicolons = 0;

    /* A vertical bar is unambiguous even when legacy matrices omit their
       brackets and therefore contain top-level row semicolons. */
    for (index = 0; text[index] != '\0'; index++) {
        char value = text[index];
        if (value == '(' || value == '[')
            depth++;
        else if (value == ')' || value == ']') {
            if (depth == 0)
                return 0;
            depth--;
        } else if (depth == 0 && value == '|') {
            pipes++;
            separator = index;
        } else if (depth == 0 && value == ';') {
            semicolons++;
            if (pipes == 0)
                separator = index;
        }
    }
    if (depth != 0 || pipes > 1 || (pipes == 0 && semicolons != 1) ||
        separator == 0 || text[separator + 1] == '\0' ||
        separator >= MATRIX_TEXT_CAPACITY ||
        strlen(text + separator + 1) >= MATRIX_TEXT_CAPACITY)
        return 0;
    memcpy(left, text, separator);
    left[separator] = '\0';
    strcpy(right, text + separator + 1);
    return 1;
}

static u8 matrixDeterminant(const MatrixValue *matrix,
                            long double *determinant)
{
    long double working[4][4];
    long double result = 1.0L;
    u8 pivot;
    u8 row;
    u8 column;

    if (matrix->rows != matrix->columns || matrix->rows == 0)
        return CALC_ERR_DOMAIN;
    memcpy(working, matrix->cell, sizeof(working));
    for (pivot = 0; pivot < matrix->rows; pivot++) {
        u8 best = pivot;
        for (row = (u8)(pivot + 1); row < matrix->rows; row++)
            if (fabsl(working[row][pivot]) > fabsl(working[best][pivot]))
                best = row;
        if (fabsl(working[best][pivot]) < 1e-18L) {
            *determinant = 0.0L;
            return CALC_OK;
        }
        if (best != pivot) {
            for (column = 0; column < matrix->columns; column++) {
                long double swap = working[pivot][column];
                working[pivot][column] = working[best][column];
                working[best][column] = swap;
            }
            result = -result;
        }
        result *= working[pivot][pivot];
        for (row = (u8)(pivot + 1); row < matrix->rows; row++) {
            long double factor = working[row][pivot] / working[pivot][pivot];
            for (column = (u8)(pivot + 1); column < matrix->columns; column++)
                working[row][column] -= factor * working[pivot][column];
        }
    }
    *determinant = result;
    return CALC_OK;
}

static u8 matrixInverse(const MatrixValue *matrix, MatrixValue *inverse)
{
    long double augmented[4][8];
    u8 size;
    u8 row;
    u8 column;
    u8 pivot;

    if (matrix->rows != matrix->columns || matrix->rows == 0)
        return CALC_ERR_DOMAIN;
    size = matrix->rows;
    memset(augmented, 0, sizeof(augmented));
    for (row = 0; row < size; row++) {
        for (column = 0; column < size; column++)
            augmented[row][column] = matrix->cell[row][column];
        augmented[row][size + row] = 1.0L;
    }
    for (pivot = 0; pivot < size; pivot++) {
        u8 best = pivot;
        long double divisor;
        for (row = (u8)(pivot + 1); row < size; row++)
            if (fabsl(augmented[row][pivot]) >
                fabsl(augmented[best][pivot]))
                best = row;
        if (fabsl(augmented[best][pivot]) < 1e-18L)
            return CALC_ERR_DOMAIN;
        if (best != pivot) {
            for (column = 0; column < size * 2; column++) {
                long double swap = augmented[pivot][column];
                augmented[pivot][column] = augmented[best][column];
                augmented[best][column] = swap;
            }
        }
        divisor = augmented[pivot][pivot];
        for (column = 0; column < size * 2; column++)
            augmented[pivot][column] /= divisor;
        for (row = 0; row < size; row++) {
            long double factor;
            if (row == pivot)
                continue;
            factor = augmented[row][pivot];
            for (column = 0; column < size * 2; column++)
                augmented[row][column] -= factor * augmented[pivot][column];
        }
    }
    memset(inverse, 0, sizeof(*inverse));
    inverse->rows = size;
    inverse->columns = size;
    for (row = 0; row < size; row++)
        for (column = 0; column < size; column++)
            inverse->cell[row][column] = augmented[row][size + column];
    return CALC_OK;
}

static u8 matrixBinary(const MatrixValue *left, const MatrixValue *right,
                       char operation, MatrixValue *result)
{
    u8 row;
    u8 column;
    u8 inner;

    memset(result, 0, sizeof(*result));
    if (operation == '+' || operation == '-') {
        if (left->rows != right->rows || left->columns != right->columns)
            return CALC_ERR_DOMAIN;
        result->rows = left->rows;
        result->columns = left->columns;
        for (row = 0; row < result->rows; row++)
            for (column = 0; column < result->columns; column++)
                result->cell[row][column] = left->cell[row][column] +
                    (operation == '+' ? right->cell[row][column] :
                                        -right->cell[row][column]);
        return CALC_OK;
    }
    if (left->columns != right->rows)
        return CALC_ERR_DOMAIN;
    result->rows = left->rows;
    result->columns = right->columns;
    for (row = 0; row < result->rows; row++)
        for (column = 0; column < result->columns; column++)
            for (inner = 0; inner < left->columns; inner++)
                result->cell[row][column] += left->cell[row][inner] *
                                             right->cell[inner][column];
    return CALC_OK;
}

static void matrixTranspose(const MatrixValue *input, MatrixValue *output)
{
    u8 row;
    u8 column;
    memset(output, 0, sizeof(*output));
    output->rows = input->columns;
    output->columns = input->rows;
    for (row = 0; row < input->rows; row++)
        for (column = 0; column < input->columns; column++)
            output->cell[column][row] = input->cell[row][column];
}

static u8 formatMatrix(const MatrixValue *matrix, char *output, u16 capacity)
{
    u16 used = 0;
    u8 row;
    u8 column;

    if (capacity < 3)
        return 0;
    output[used++] = '[';
    output[used] = '\0';
    for (row = 0; row < matrix->rows; row++) {
        if (row != 0) {
            if (used + 2 >= capacity)
                return 0;
            output[used++] = ';';
        }
        for (column = 0; column < matrix->columns; column++) {
            char value[28];
            u16 length;
            if (column != 0) {
                if (used + 2 >= capacity)
                    return 0;
                output[used++] = ',';
            }
            modeFormatReal(matrix->cell[row][column], value,
                           sizeof(value), 7);
            length = (u16)strlen(value);
            if (used + length + 2 >= capacity)
                return 0;
            memcpy(output + used, value, length);
            used = (u16)(used + length);
        }
    }
    output[used++] = ']';
    output[used] = '\0';
    return 1;
}

u8 modeMatrixEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[20];
    char arguments[MATRIX_BINARY_ARGUMENT_CAPACITY];
    const char *body = expression;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));
    MatrixValue left;
    MatrixValue right;
    MatrixValue result;
    u8 error;
    char output[MODE_RESULT_CAPACITY];

    if (!command || modeEquals(name, "det") ||
        modeEquals(name, "determinant")) {
        long double determinant;
        body = command ? arguments : expression;
        error = parseMatrix(runtime, body, &left);
        if (error != CALC_OK)
            return error;
        error = matrixDeterminant(&left, &determinant);
        if (error != CALC_OK)
            return error;
        modeFormatReal(determinant, output, sizeof(output), 9);
        modeSetResult(runtime, output, CALC_OK);
        return CALC_OK;
    }
    body = arguments;
    if (modeEquals(name, "inv") || modeEquals(name, "inverse")) {
        error = parseMatrix(runtime, body, &left);
        if (error != CALC_OK)
            return error;
        error = matrixInverse(&left, &result);
    } else if (modeEquals(name, "tr") || modeEquals(name, "transpose")) {
        error = parseMatrix(runtime, body, &left);
        if (error != CALC_OK)
            return error;
        matrixTranspose(&left, &result);
        error = CALC_OK;
    } else if (modeEquals(name, "add") || modeEquals(name, "sub") ||
               modeEquals(name, "mul")) {
        char leftText[MATRIX_TEXT_CAPACITY];
        char rightText[MATRIX_TEXT_CAPACITY];
        if (!splitMatrixPair(body, leftText, rightText))
            return CALC_ERR_SYNTAX;
        error = parseMatrix(runtime, leftText, &left);
        if (error != CALC_OK)
            return error;
        error = parseMatrix(runtime, rightText, &right);
        if (error != CALC_OK)
            return error;
        error = matrixBinary(&left, &right,
                             modeEquals(name, "add") ? '+' :
                             modeEquals(name, "sub") ? '-' : '*',
                             &result);
    } else
        return CALC_ERR_SYNTAX;
    if (error != CALC_OK)
        return error;
    if (!formatMatrix(&result, output, sizeof(output)))
        return CALC_ERR_RANGE;
    modeSetResult(runtime, output, CALC_OK);
    return CALC_OK;
}
