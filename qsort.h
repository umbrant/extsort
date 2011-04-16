#ifndef QSORT_H
#define QSORT_H

#include "util.h"


void ext_qsort(run_t* data_ptr);
void parallel_qsort(run_t** runs, int num_runs);

typedef struct worker_data {
    int offset;
    int length;
    run_t** runs;
} worker_data_t;

#endif
