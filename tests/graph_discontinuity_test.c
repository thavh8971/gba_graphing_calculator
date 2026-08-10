#include <stdio.h>

#include "gcalc/graph.h"

static int fail(const char *message)
{
    fprintf(stderr, "FAIL graph discontinuity: %s\n", message);
    return 1;
}

int main(void)
{
    CalcContext context;
    GraphFunction function[1];
    GraphViewport viewport;
    GraphJob job;
    GraphSample samples[16];
    u8 count;
    u8 produced;
    u8 index;
    u8 sawPole = 0;
    u8 sawDomain = 0;

    calcContextInit(&context);
    viewport.xMin = CALC_ONE;
    viewport.xMax = 2 * CALC_ONE;
    viewport.yMin = -4 * CALC_ONE;
    viewport.yMax = 4 * CALC_ONE;
    if (graphParseFunctions("tan(x)", function, 1, &count) != CALC_OK ||
        count != 1 || !function[0].hasTangent)
        return fail("tan row was not classified");
    graphJobBegin(&job, &context, function, count, viewport, 9);
    if (graphJobStep(&job, samples, 16, &produced) != CALC_OK || produced != 9)
        return fail("tan sampling did not finish one function");
    for (index = 0; index < produced; index++) {
        if (samples[index].state == GRAPH_SAMPLE_POLE &&
            samples[index].breakBefore)
            sawPole = 1;
    }
    if (!sawPole)
        return fail("phase crossing around pi/2 was connected");

    viewport.xMin = -2 * CALC_ONE;
    viewport.xMax = 2 * CALC_ONE;
    if (graphParseFunctions("sqrt(x)", function, 1, &count) != CALC_OK)
        return fail("sqrt row parse");
    graphJobBegin(&job, &context, function, count, viewport, 9);
    if (graphJobStep(&job, samples, 16, &produced) != CALC_OK)
        return fail("sqrt sampling");
    for (index = 0; index < produced; index++) {
        if (samples[index].state == GRAPH_SAMPLE_DOMAIN_ERROR &&
            samples[index].breakBefore)
            sawDomain = 1;
    }
    if (!sawDomain)
        return fail("invalid sqrt branch was not broken");

    puts("graph_discontinuity_test: PASS");
    return 0;
}
