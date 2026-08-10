#include <string.h>
#include <math.h>
#include <limits.h>

#include "gcalc/graph.h"

static u8 isSpace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static void trimCopy(char *destination, u16 capacity,
                     const char *source, u16 length)
{
    u16 start = 0;
    u16 end = length;
    u16 output = 0;

    while (start < end && isSpace(source[start]))
        start++;
    while (end > start && isSpace(source[end - 1]))
        end--;
    while (start < end && output + 1 < capacity)
        destination[output++] = source[start++];
    if (capacity != 0)
        destination[output] = '\0';
}

static u8 startsWithInsensitive(const char *text, const char *prefix)
{
    while (isSpace(*text))
        text++;
    while (*prefix != '\0') {
        if (*text == '\0')
            return 0;
        char left = *text++;
        char right = *prefix++;
        if (left >= 'A' && left <= 'Z')
            left = (char)(left - 'A' + 'a');
        if (right >= 'A' && right <= 'Z')
            right = (char)(right - 'A' + 'a');
        if (left != right)
            return 0;
    }
    return 1;
}

static u8 astNeedsBridge(const CalcSyntaxAst *ast)
{
    static const char *const guardedCalls[] = {
        "sqrt", "ln", "log", "log10", "lg", "logab",
        "asin", "acos", "sin^-1", "cos^-1", "acosh", "atanh",
        "root", "nroot", "pow", "recip", "sec", "csc", "cot",
        "csch", "coth", "mod", "rmdr", "intdiv"
    };
    u8 index;

    if (calcSyntaxAstHasOperator(ast, '/') ||
        calcSyntaxAstHasOperator(ast, '^'))
        return 1;
    for (index = 0; index < (u8)(sizeof(guardedCalls) /
                                  sizeof(guardedCalls[0])); index++)
        if (calcSyntaxAstHasCall(ast, guardedCalls[index]))
            return 1;
    return 0;
}

static u8 formulaNeedsBridge(const char *formula)
{
    CalcSyntaxAst ast;
    return calcSyntaxParse(formula, &ast) == CALC_OK &&
           astNeedsBridge(&ast);
}

static u8 appendGraphFunction(const char *start, u16 length,
                              GraphFunction *functions, u8 capacity,
                              u8 *count)
{
    GraphFunction *function;
    u8 error;
    u8 whole;

    if (*count >= capacity || length == 0 || length >= 128)
        return 0;
    function = &functions[*count];
    memset(function, 0, sizeof(*function));
    trimCopy(function->source, sizeof(function->source), start, length);
    if (function->source[0] == '\0')
        return 0;
    error = calcSyntaxParseGraphRow(function->source, &function->row);
    if (error != CALC_OK) {
        CalcSyntaxAst validation;
        if (calcSyntaxParse(function->source, &validation) != CALC_OK)
            return 0;
        if (strlen(function->source) >= sizeof(function->row.formulaA))
            return 0;
        memset(&function->row, 0, sizeof(function->row));
        function->row.kind = CALC_GRAPH_ROW_Y;
        trimCopy(function->row.formulaA, sizeof(function->row.formulaA),
                 function->source, (u16)strlen(function->source));
    }
    function->hasTangent = calcSyntaxFindCall(
        function->row.formulaA, "tan", function->tangentPhase,
        sizeof(function->tangentPhase), &whole);
    if (function->row.kind == CALC_GRAPH_ROW_PARAM &&
        calcSyntaxFindCall(function->row.formulaB, "tan",
                           function->tangentPhase,
                           sizeof(function->tangentPhase), &whole))
        function->hasTangent = 1;
    function->needsBridge = formulaNeedsBridge(function->row.formulaA);
    if (function->row.kind == CALC_GRAPH_ROW_PARAM &&
        formulaNeedsBridge(function->row.formulaB))
        function->needsBridge = 1;
    function->enabled = 1;
    function->valid = 1;
    (*count)++;
    return 1;
}

u8 graphParseFunctions(const char *source, GraphFunction *functions,
                       u8 capacity, u8 *count)
{
    u16 index;
    u16 rowStart;
    s16 depth;
    u8 parsed = 0;

    if (source == 0 || functions == 0 || count == 0 || capacity == 0)
        return CALC_ERR_SYNTAX;
    *count = 0;
    rowStart = 0;
    depth = 0;
    for (index = 0;; index++) {
        char value = source[index];

        if (value == '(' || value == '[')
            depth++;
        else if ((value == ')' || value == ']') && depth > 0)
            depth--;

        /* The semicolon between the two parametric components belongs to
           that row. A following top-level semicolon starts another row. */
        if ((value == ';' && depth == 0) || value == '\0') {
            u8 isParam = startsWithInsensitive(source + rowStart, "param:");
            if (value == ';' && isParam && !parsed) {
                parsed = 1;
                continue;
            }
            if (!appendGraphFunction(source + rowStart,
                                     (u16)(index - rowStart), functions,
                                     capacity, count)) {
                *count = 0;
                return CALC_ERR_SYNTAX;
            }
            rowStart = (u16)(index + 1);
            parsed = 0;
        }
        if (value == '\0')
            break;
    }
    return *count != 0 ? CALC_OK : CALC_ERR_SYNTAX;
}

static GraphSampleState errorToSample(u8 error)
{
    if (error == CALC_OK)
        return GRAPH_SAMPLE_VALID;
    if (error == CALC_ERR_DOMAIN || error == CALC_ERR_DIVZERO)
        return GRAPH_SAMPLE_DOMAIN_ERROR;
    return GRAPH_SAMPLE_OVERFLOW;
}

static u8 evaluateFixed(CalcContext *context, const char *formula,
                        char variable, s32 parameterFixed, s32 *value)
{
    CalcNumber saved;
    CalcNumber savedAnswer = context->answer;
    CalcNumber savedPreviousAnswer = context->previousAnswer;
    CalcNumber result;
    CalcNumber parameter = calcNumberFromFixed(parameterFixed);
    u8 hadSaved;
    u8 hadAnswer = context->hasAnswer;
    u8 hadPreviousAnswer = context->hasPreviousAnswer;
    u8 error;

    hadSaved = calcContextGetVariable(context, variable, &saved);
    calcContextSetVariable(context, variable, parameter);
    calcEvaluateContext(context, formula, &result, &error);
    if (hadSaved)
        calcContextSetVariable(context, variable, saved);
    else
        context->variableMask &= ~(1UL << (variable - 'A'));
    context->answer = savedAnswer;
    context->previousAnswer = savedPreviousAnswer;
    context->hasAnswer = hadAnswer;
    context->hasPreviousAnswer = hadPreviousAnswer;
    if (error != CALC_OK)
        return error;
    return calcNumberToFixed(result, value);
}

static u8 evaluateNumberWithVariable(CalcContext *context,
                                     const char *formula, char variable,
                                     s32 parameterFixed,
                                     CalcNumber *result)
{
    CalcNumber saved;
    CalcNumber savedAnswer = context->answer;
    CalcNumber savedPreviousAnswer = context->previousAnswer;
    CalcNumber parameter = calcNumberFromFixed(parameterFixed);
    u8 hadSaved;
    u8 hadAnswer = context->hasAnswer;
    u8 hadPreviousAnswer = context->hasPreviousAnswer;
    u8 error;

    hadSaved = calcContextGetVariable(context, variable, &saved);
    calcContextSetVariable(context, variable, parameter);
    calcEvaluateContext(context, formula, result, &error);
    if (hadSaved)
        calcContextSetVariable(context, variable, saved);
    else
        context->variableMask &= ~(1UL << (variable - 'A'));
    context->answer = savedAnswer;
    context->previousAnswer = savedPreviousAnswer;
    context->hasAnswer = hadAnswer;
    context->hasPreviousAnswer = hadPreviousAnswer;
    return error;
}

static long double parameterRadians(const CalcContext *context,
                                    s32 parameterFixed)
{
    long double value = (long double)parameterFixed / CALC_ONE;
    if (context->angleMode == CALC_ANGLE_DEG)
        return value * 3.1415926535897932384626433832795L / 180.0L;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return value * 3.1415926535897932384626433832795L / 200.0L;
    return value;
}

GraphSampleState graphEvaluatePoint(const GraphFunction *function,
                                    CalcContext *context,
                                    s32 parameterFixed, s32 *xFixed,
                                    s32 *yFixed)
{
    u8 error;

    if (function == 0 || context == 0 || xFixed == 0 || yFixed == 0 ||
        !function->valid)
        return GRAPH_SAMPLE_DOMAIN_ERROR;
    switch (function->row.kind) {
    case CALC_GRAPH_ROW_X:
        *yFixed = parameterFixed;
        error = evaluateFixed(context, function->row.formulaA, 'Y',
                              parameterFixed, xFixed);
        break;
    case CALC_GRAPH_ROW_POLAR: {
        s32 radiusFixed;
        long double angle;
        long double radius;
        long double x;
        long double y;

        error = evaluateFixed(context, function->row.formulaA, 'T',
                              parameterFixed, &radiusFixed);
        if (error != CALC_OK)
            break;
        angle = parameterRadians(context, parameterFixed);
        radius = (long double)radiusFixed / CALC_ONE;
        x = radius * cosl(angle) * CALC_ONE;
        y = radius * sinl(angle) * CALC_ONE;
        if (x < -2147483647.0L || x > 2147483647.0L ||
            y < -2147483647.0L || y > 2147483647.0L)
            return GRAPH_SAMPLE_OVERFLOW;
        *xFixed = (s32)(x >= 0 ? x + 0.5L : x - 0.5L);
        *yFixed = (s32)(y >= 0 ? y + 0.5L : y - 0.5L);
        return GRAPH_SAMPLE_VALID;
    }
    case CALC_GRAPH_ROW_PARAM:
        error = evaluateFixed(context, function->row.formulaA, 'T',
                              parameterFixed, xFixed);
        if (error == CALC_OK)
            error = evaluateFixed(context, function->row.formulaB, 'T',
                                  parameterFixed, yFixed);
        break;
    case CALC_GRAPH_ROW_INEQUALITY:
        if (function->row.axisX) {
            *yFixed = parameterFixed;
            error = evaluateFixed(context, function->row.formulaA, 'Y',
                                  parameterFixed, xFixed);
        } else {
            *xFixed = parameterFixed;
            error = evaluateFixed(context, function->row.formulaA, 'X',
                                  parameterFixed, yFixed);
        }
        break;
    case CALC_GRAPH_ROW_Y:
    default:
        *xFixed = parameterFixed;
        error = evaluateFixed(context, function->row.formulaA, 'X',
                              parameterFixed, yFixed);
        break;
    }
    return errorToSample(error);
}

static void skipInactiveFunctions(GraphJob *job)
{
    while (job->functionIndex < job->functionCount &&
           (!job->functions[job->functionIndex].enabled ||
            !job->functions[job->functionIndex].valid)) {
        job->functionIndex++;
        job->nextSample = 0;
        job->hasPrevious = 0;
    }
    if (job->functionIndex >= job->functionCount)
        job->complete = 1;
}

void graphJobBegin(GraphJob *job, CalcContext *context,
                   const GraphFunction *functions, u8 count,
                   GraphViewport viewport, s32 sampleCount)
{
    if (job == 0)
        return;
    memset(job, 0, sizeof(*job));
    if (functions == 0)
        count = 0;
    if (count > GRAPH_MAX_FUNCTIONS)
        count = GRAPH_MAX_FUNCTIONS;
    job->functions = functions;
    job->context = context;
    job->viewport = viewport;
    job->functionCount = count;
    job->sampleCount = sampleCount < 2 ? 2 : sampleCount;
    job->complete = count == 0;
    if (!job->complete)
        skipInactiveFunctions(job);
}

static s32 interpolateFixed(s32 minimum, s32 maximum, s32 index, s32 count)
{
    s64 distance = (s64)maximum - minimum;
    return (s32)(minimum + distance * index / (count - 1));
}

static s32 fullTurnFixed(const CalcContext *context)
{
    if (context->angleMode == CALC_ANGLE_DEG)
        return 360 * CALC_ONE;
    if (context->angleMode == CALC_ANGLE_GRAD)
        return 400 * CALC_ONE;
    return (s32)(6.2831853071795864769L * CALC_ONE + 0.5L);
}

static u8 formulaHasTangent(const GraphFunction *function)
{
    return function->hasTangent;
}

static u8 tangentPhaseCrosses(const CalcContext *context,
                              CalcNumber phase0, CalcNumber phase1)
{
    const long double pi = 3.1415926535897932384626433832795L;
    long double left = calcNumberToLongDouble(phase0);
    long double right = calcNumberToLongDouble(phase1);
    long double low;
    long double high;
    long double firstIndex;
    long double pole;

    if (context->angleMode == CALC_ANGLE_DEG) {
        left *= pi / 180.0L;
        right *= pi / 180.0L;
    } else if (context->angleMode == CALC_ANGLE_GRAD) {
        left *= pi / 200.0L;
        right *= pi / 200.0L;
    }
    low = left < right ? left : right;
    high = left < right ? right : left;
    firstIndex = ceill((low - pi * 0.5L) / pi);
    pole = pi * 0.5L + firstIndex * pi;
    return (u8)(pole >= low && pole <= high);
}

static s64 absolute64(s64 value)
{
    return value < 0 ? -value : value;
}

static u8 bridgePointReasonable(const GraphViewport *viewport,
                                s32 x0, s32 y0, s32 x1, s32 y1,
                                s32 middleX, s32 middleY)
{
    s64 expectedX = ((s64)x0 + x1) / 2;
    s64 expectedY = ((s64)y0 + y1) / 2;
    s64 xSpan = (s64)viewport->xMax - viewport->xMin;
    s64 ySpan = (s64)viewport->yMax - viewport->yMin;
    s64 xLimit;
    s64 yLimit;

    if (xSpan < 0)
        xSpan = -xSpan;
    if (ySpan < 0)
        ySpan = -ySpan;
    xLimit = xSpan > (INT64_MAX - CALC_ONE) / 4 ? INT64_MAX :
             xSpan * 4 + CALC_ONE;
    yLimit = ySpan > (INT64_MAX - CALC_ONE) / 4 ? INT64_MAX :
             ySpan * 4 + CALC_ONE;
    return absolute64((s64)middleX - expectedX) <= xLimit &&
           absolute64((s64)middleY - expectedY) <= yLimit;
}

static u8 bridgeIntervalSafe(const GraphFunction *function,
                             CalcContext *context,
                             const GraphViewport *viewport,
                             s32 parameter0, s32 x0, s32 y0,
                             s32 parameter1, s32 x1, s32 y1,
                             u8 depth)
{
    s32 middleParameter = (s32)((s64)parameter0 +
                                 ((s64)parameter1 - parameter0) / 2);
    s32 middleX;
    s32 middleY;

    if (middleParameter == parameter0 || middleParameter == parameter1)
        return 1;
    if (graphEvaluatePoint(function, context, middleParameter,
                           &middleX, &middleY) != GRAPH_SAMPLE_VALID ||
        !bridgePointReasonable(viewport, x0, y0, x1, y1,
                               middleX, middleY))
        return 0;
    if (depth <= 1)
        return 1;
    return bridgeIntervalSafe(function, context, viewport,
                              parameter0, x0, y0,
                              middleParameter, middleX, middleY,
                              (u8)(depth - 1)) &&
           bridgeIntervalSafe(function, context, viewport,
                              middleParameter, middleX, middleY,
                              parameter1, x1, y1, (u8)(depth - 1));
}

u8 graphJobStep(GraphJob *job, GraphSample *output, u8 capacity,
                u8 *produced)
{
    if (produced == 0)
        return CALC_ERR_SYNTAX;
    *produced = 0;
    if (job == 0 || output == 0 || capacity == 0 || job->context == 0)
        return CALC_ERR_SYNTAX;
    if (job->functions == 0) {
        job->complete = 1;
        return CALC_OK;
    }
    skipInactiveFunctions(job);
    while (*produced < capacity && !job->complete) {
        const GraphFunction *function = &job->functions[job->functionIndex];
        GraphSample *sample = &output[*produced];
        s32 minimum;
        s32 maximum;

        if (function->row.kind == CALC_GRAPH_ROW_X ||
            (function->row.kind == CALC_GRAPH_ROW_INEQUALITY &&
             function->row.axisX)) {
            minimum = job->viewport.yMin;
            maximum = job->viewport.yMax;
        } else if (function->row.kind == CALC_GRAPH_ROW_POLAR ||
                   function->row.kind == CALC_GRAPH_ROW_PARAM) {
            minimum = 0;
            maximum = fullTurnFixed(job->context);
        } else {
            minimum = job->viewport.xMin;
            maximum = job->viewport.xMax;
        }
        memset(sample, 0, sizeof(*sample));
        sample->parameterFixed = interpolateFixed(minimum, maximum,
                                                  job->nextSample,
                                                  job->sampleCount);
        sample->state = graphEvaluatePoint(function, job->context,
                                           sample->parameterFixed,
                                           &sample->xFixed,
                                           &sample->yFixed);
        if (function->hasTangent) {
            char variable = 'X';
            if (function->row.kind == CALC_GRAPH_ROW_X ||
                (function->row.kind == CALC_GRAPH_ROW_INEQUALITY &&
                 function->row.axisX))
                variable = 'Y';
            else if (function->row.kind == CALC_GRAPH_ROW_POLAR ||
                     function->row.kind == CALC_GRAPH_ROW_PARAM)
                variable = 'T';
            if (evaluateNumberWithVariable(job->context,
                                           function->tangentPhase,
                                           variable,
                                           sample->parameterFixed,
                                           &sample->phaseNumber) == CALC_OK) {
                sample->phaseValid = 1;
                if (calcNumberToFixed(sample->phaseNumber,
                                      &sample->phaseFixed) != CALC_OK)
                    sample->phaseFixed = 0;
            }
        }
        sample->breakBefore = !job->hasPrevious ||
                              sample->state != GRAPH_SAMPLE_VALID ||
                              job->previous.state != GRAPH_SAMPLE_VALID;
        if (!sample->breakBefore && function->needsBridge &&
            !bridgeIntervalSafe(function, job->context, &job->viewport,
                                job->previous.parameterFixed,
                                job->previous.xFixed,
                                job->previous.yFixed,
                                sample->parameterFixed,
                                sample->xFixed, sample->yFixed, 2))
            sample->breakBefore = 1;
        if (!sample->breakBefore && formulaHasTangent(function) &&
            job->previous.phaseValid && sample->phaseValid) {
            if (tangentPhaseCrosses(job->context,
                                    job->previous.phaseNumber,
                                    sample->phaseNumber)) {
                sample->state = GRAPH_SAMPLE_POLE;
                sample->breakBefore = 1;
            }
        }
        job->previous = *sample;
        job->hasPrevious = 1;
        (*produced)++;
        job->nextSample++;
        if (job->nextSample >= job->sampleCount) {
            job->nextSample = 0;
            job->hasPrevious = 0;
            job->functionIndex++;
            skipInactiveFunctions(job);
        }
    }
    return CALC_OK;
}

static u8 clipCode(s32 x, s32 y, s32 xMin, s32 xMax,
                   s32 yMin, s32 yMax)
{
    u8 code = 0;
    if (x < xMin)
        code |= 1;
    else if (x > xMax)
        code |= 2;
    if (y < yMin)
        code |= 4;
    else if (y > yMax)
        code |= 8;
    return code;
}

static s32 clipIntersectionCoordinate(s32 origin, s32 other,
                                      s64 boundaryDelta, s64 axisDelta)
{
    long double result = origin + ((long double)other - origin) *
                         boundaryDelta / axisDelta;
    if (result <= (long double)INT32_MIN)
        return INT32_MIN;
    if (result >= (long double)INT32_MAX)
        return INT32_MAX;
    return (s32)result;
}

u8 graphClipWorldSegment(s32 x0, s32 y0, s32 x1, s32 y1,
                         s32 xMin, s32 xMax, s32 yMin, s32 yMax,
                         s32 *clippedX0, s32 *clippedY0,
                         s32 *clippedX1, s32 *clippedY1)
{
    u8 code0;
    u8 code1;
    u8 iterations = 0;

    if (clippedX0 == 0 || clippedY0 == 0 || clippedX1 == 0 ||
        clippedY1 == 0 || xMin > xMax || yMin > yMax)
        return 0;
    code0 = clipCode(x0, y0, xMin, xMax, yMin, yMax);
    code1 = clipCode(x1, y1, xMin, xMax, yMin, yMax);
    while (iterations++ < 12) {
        u8 outside;
        s32 x;
        s32 y;

        if ((code0 | code1) == 0) {
            *clippedX0 = x0;
            *clippedY0 = y0;
            *clippedX1 = x1;
            *clippedY1 = y1;
            return 1;
        }
        if ((code0 & code1) != 0)
            return 0;
        outside = code0 != 0 ? code0 : code1;
        if (outside & 8) {
            if (y1 == y0)
                return 0;
            x = clipIntersectionCoordinate(x0, x1, (s64)yMax - y0,
                                           (s64)y1 - y0);
            y = yMax;
        } else if (outside & 4) {
            if (y1 == y0)
                return 0;
            x = clipIntersectionCoordinate(x0, x1, (s64)yMin - y0,
                                           (s64)y1 - y0);
            y = yMin;
        } else if (outside & 2) {
            if (x1 == x0)
                return 0;
            y = clipIntersectionCoordinate(y0, y1, (s64)xMax - x0,
                                           (s64)x1 - x0);
            x = xMax;
        } else {
            if (x1 == x0)
                return 0;
            y = clipIntersectionCoordinate(y0, y1, (s64)xMin - x0,
                                           (s64)x1 - x0);
            x = xMin;
        }
        if (outside == code0) {
            x0 = x;
            y0 = y;
            code0 = clipCode(x0, y0, xMin, xMax, yMin, yMax);
        } else {
            x1 = x;
            y1 = y;
            code1 = clipCode(x1, y1, xMin, xMax, yMin, yMax);
        }
    }
    return 0;
}
