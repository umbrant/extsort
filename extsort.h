#ifndef EXTSORT_H
#define EXTSORT_H

#include "util.h"
#include "qsort.h"

int main(int argc, char* argv[]);
void usage();
void error(const char* msg);
void multimerge(run_t** runs, char* input_prefix, int num_runs, int output_fd, int base);
void verify(char* filename);

#endif
