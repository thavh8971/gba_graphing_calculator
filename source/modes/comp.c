#include "mode_internal.h"

static u8 specialModeCall(const char *name)
{
    return modeEquals(name, "binom") || modeEquals(name, "binompdf") ||
           modeEquals(name, "binomcdf") || modeEquals(name, "poisson") ||
           modeEquals(name, "poissonpdf") ||
           modeEquals(name, "poissoncdf") || modeEquals(name, "normpdf") ||
           modeEquals(name, "normcdf") || modeEquals(name, "norminv") ||
           modeEquals(name, "normalpdf") ||
           modeEquals(name, "normalcdf") ||
           modeEquals(name, "normalinv") || modeEquals(name, "geompdf") ||
           modeEquals(name, "geomcdf") ||
           modeEquals(name, "geometricpdf") ||
           modeEquals(name, "geometriccdf") ||
           modeEquals(name, "hypergeom") ||
           modeEquals(name, "hypergeometric") ||
           modeEquals(name, "hypergeompdf");
}

u8 modeCompEvaluate(ModeRuntime *runtime, const char *expression)
{
    char name[16];
    char arguments[160];
    CalcNumber value;
    char buffer[64];
    u8 error;

    if (modeParseCommand(expression, name, sizeof(name), arguments,
                         sizeof(arguments))) {
        if (modeEquals(name, "solve") || modeEquals(name, "solven"))
            return modeEquationEvaluate(runtime, expression);
        if (specialModeCall(name))
            return CALC_ERR_SYNTAX;
        if (modeEquals(name, "calc")) {
            char nested[160];
            expression = arguments;
            if (modeParseCommand(expression, name, sizeof(name), nested,
                                 sizeof(nested)) && specialModeCall(name))
                return CALC_ERR_SYNTAX;
        }
    }
    calcEvaluateContext(&runtime->calc, expression, &value, &error);
    if (error != CALC_OK)
        return error;
    calcFormatNumber(value, buffer, sizeof(buffer));
    modeSetResult(runtime, buffer, CALC_OK);
    return CALC_OK;
}
