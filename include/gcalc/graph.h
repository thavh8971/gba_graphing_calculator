#ifndef GCALC_GRAPH_H
#define GCALC_GRAPH_H

#include "gcalc/calc.h"
#include "gcalc/syntax.h"

#define GRAPH_MAX_FUNCTIONS 10
#define GRAPH_CHUNK_SAMPLES 40

typedef enum GraphSampleState {
    GRAPH_SAMPLE_VALID,
    GRAPH_SAMPLE_DOMAIN_ERROR,
    GRAPH_SAMPLE_POLE,
    GRAPH_SAMPLE_OVERFLOW
} GraphSampleState;

typedef struct GraphSample {
    s32 xFixed;
    s32 yFixed;
    s32 parameterFixed;
    s32 phaseFixed;
    CalcNumber phaseNumber;
    GraphSampleState state;
    u8 phaseValid;
    u8 breakBefore;
} GraphSample;

typedef struct GraphFunction {
    CalcSyntaxGraphRow row;
    char source[128];
    char tangentPhase[CALC_SYNTAX_TEXT];
    u8 enabled;
    u8 valid;
    u8 hasTangent;
    u8 needsBridge;
} GraphFunction;

typedef struct GraphViewport {
    s32 xMin;
    s32 xMax;
    s32 yMin;
    s32 yMax;
} GraphViewport;

typedef struct GraphJob {
    const GraphFunction *functions;
    GraphViewport viewport;
    CalcContext *context;
    s32 sampleCount;
    s32 nextSample;
    GraphSample previous;
    u8 functionCount;
    u8 functionIndex;
    u8 hasPrevious;
    u8 complete;
} GraphJob;

u8 graphParseFunctions(const char *source, GraphFunction *functions,
                       u8 capacity, u8 *count);
void graphJobBegin(GraphJob *job, CalcContext *context,
                   const GraphFunction *functions, u8 count,
                   GraphViewport viewport, s32 sampleCount);
u8 graphJobStep(GraphJob *job, GraphSample *output, u8 capacity,
                u8 *produced);
GraphSampleState graphEvaluatePoint(const GraphFunction *function,
                                    CalcContext *context,
                                    s32 parameterFixed, s32 *xFixed,
                                    s32 *yFixed);
u8 graphClipWorldSegment(s32 x0, s32 y0, s32 x1, s32 y1,
                         s32 xMin, s32 xMax, s32 yMin, s32 yMax,
                         s32 *clippedX0, s32 *clippedY0,
                         s32 *clippedX1, s32 *clippedY1);

#endif
