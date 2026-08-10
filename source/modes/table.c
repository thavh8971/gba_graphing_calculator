#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mode_internal.h"

static u8 tableEvaluateAt(ModeRuntime *runtime, const char *formula,
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
    error = modeEvalReal(runtime, formula, value);
    if (hadSaved)
        calcContextSetVariable(&runtime->calc, 'X', saved);
    else
        runtime->calc.variableMask &= ~(1UL << ('X' - 'A'));
    return error;
}

u8 modeTableEvaluate(ModeRuntime *runtime, const char *expression)
{
    char fields[5][64];
    char name[16];
    char arguments[256];
    const char *body = expression;
    long double start;
    long double end;
    long double step;
    u8 count;
    u8 formulaCount;
    u8 error;
    u8 row = 0;
    u8 derivative = 0;
    u8 command = modeParseCommand(expression, name, sizeof(name), arguments,
                                  sizeof(arguments));

    if (command) {
        if (modeEquals(name, "dtable"))
            derivative = 1;
        else if (!modeEquals(name, "table") && !modeEquals(name, "table2"))
            return CALC_ERR_SYNTAX;
        body = arguments;
    } else if (modeStartsWith(expression, "dtable:")) {
        derivative = 1;
        body += 7;
    }
    if (!modeSplit(body, ';', fields, 5, &count))
        return CALC_ERR_SYNTAX;
    if ((!derivative && count != 4 && count != 5) ||
        (derivative && count != 4))
        return CALC_ERR_SYNTAX;
    formulaCount = count == 5 ? 2 : 1;
    if (command && modeEquals(name, "table2") && formulaCount != 2)
        return CALC_ERR_SYNTAX;
    error = modeEvalReal(runtime, fields[formulaCount], &start);
    if (error == CALC_OK)
        error = modeEvalReal(runtime, fields[formulaCount + 1], &end);
    if (error == CALC_OK)
        error = modeEvalReal(runtime, fields[formulaCount + 2], &step);
    if (error != CALC_OK)
        return error;
    if (step == 0.0L || (end > start && step < 0.0L) ||
        (end < start && step > 0.0L))
        return CALC_ERR_DOMAIN;
    runtime->table.functionCount = formulaCount;
    runtime->table.hasDerivative = derivative;
    while (row < TABLE_MAX_ROWS) {
        long double x = start + row * step;
        long double y;
        long double z = 0.0L;

        if ((step > 0.0L && x > end + fabsl(step) * 1e-12L) ||
            (step < 0.0L && x < end - fabsl(step) * 1e-12L))
            break;
        error = tableEvaluateAt(runtime, fields[0], x, &y);
        if (error != CALC_OK)
            return error;
        if (formulaCount == 2) {
            error = tableEvaluateAt(runtime, fields[1], x, &z);
            if (error != CALC_OK)
                return error;
        }
        modeFormatReal(x, runtime->table.x[row],
                       sizeof(runtime->table.x[row]), 7);
        modeFormatReal(y, runtime->table.y[row],
                       sizeof(runtime->table.y[row]), 7);
        if (formulaCount == 2)
            modeFormatReal(z, runtime->table.z[row],
                           sizeof(runtime->table.z[row]), 7);
        if (derivative) {
            long double h = (fabsl(x) + 1.0L) / 4096.0L;
            long double plus;
            long double minus;
            error = tableEvaluateAt(runtime, fields[0], x + h, &plus);
            if (error == CALC_OK)
                error = tableEvaluateAt(runtime, fields[0], x - h, &minus);
            if (error != CALC_OK)
                return error;
            modeFormatReal((plus - minus) / (2.0L * h),
                           runtime->table.derivative[row],
                           sizeof(runtime->table.derivative[row]), 7);
        }
        row++;
    }
    runtime->table.rows = row;
    if (row == 0)
        return CALC_ERR_DOMAIN;
    modeSetResult(runtime, "TABLE READY", CALC_OK);
    return CALC_OK;
}
