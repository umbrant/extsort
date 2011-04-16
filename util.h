#ifndef UTIL_H
#define UTIL_H

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>


#ifdef DEBUG
	#define PRINTF(...) printf(__VA_ARGS__)
#else
	#define PRINTF(...)
#endif

// Number of bytes in a page
#define PAGE_SIZE (4*(1<<10))
#define INTS_SIZE PAGE_SIZE/sizeof(int)

#define NUM_THREADS 8

/*
typedef struct {
    int items[PAGE_SIZE];
} page_t;
*/

typedef struct {
    int length;
    int* items;
} run_t;

void error(const char* msg);

#endif
