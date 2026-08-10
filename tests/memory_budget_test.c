#include <stdio.h>

#include "gcalc/app.h"

int main(void)
{
    if (sizeof(GraphFunction) > 512) {
        fprintf(stderr, "FAIL GraphFunction too large: %lu\n",
                (unsigned long)sizeof(GraphFunction));
        return 1;
    }
    if (sizeof(GraphJob) > 512) {
        fprintf(stderr, "FAIL GraphJob too large: %lu\n",
                (unsigned long)sizeof(GraphJob));
        return 1;
    }
    if (sizeof(AppState) > 16 * 1024) {
        fprintf(stderr, "FAIL AppState exceeds 16 KiB budget: %lu\n",
                (unsigned long)sizeof(AppState));
        return 1;
    }
    if (sizeof(GraphSample) * GRAPH_CHUNK_SAMPLES > 2048) {
        fprintf(stderr, "FAIL graph chunk exceeds 2 KiB: %lu\n",
                (unsigned long)(sizeof(GraphSample) * GRAPH_CHUNK_SAMPLES));
        return 1;
    }
    printf("memory_budget_test: PASS (AppState=%lu, GraphFunction=%lu, "
           "GraphJob=%lu, chunk=%lu)\n",
           (unsigned long)sizeof(AppState),
           (unsigned long)sizeof(GraphFunction),
           (unsigned long)sizeof(GraphJob),
           (unsigned long)(sizeof(GraphSample) * GRAPH_CHUNK_SAMPLES));
    return 0;
}
