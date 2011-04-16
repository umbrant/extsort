#include "qsort.h"

static void qsort_recurse(run_t* data_ptr, int left, int right);
static void swap(run_t* data_ptr, int first, int second);
static int partition(run_t* data_ptr, int left, int right, int pivot_index);
static void bubble(run_t* data_ptr, int left, int right);
static void* qsort_worker(void *runs_per_thread);

void ext_qsort(run_t* data_ptr)
{
    qsort_recurse(data_ptr, 0, data_ptr->length-1);
}


void parallel_qsort(run_t** runs, int num_runs)
{
    // Allocate work for each thread
    worker_data_t** runs_per_thread;
    runs_per_thread = (worker_data_t**)calloc(1, NUM_THREADS*sizeof(worker_data_t*));
    int remainder = num_runs % NUM_THREADS;
    int offset = 0;
    for(int i=0; i<NUM_THREADS; i++) {
        // Need to store the start point and count for each thread
        runs_per_thread[i] = (worker_data_t*)calloc(1, sizeof(worker_data_t));
        runs_per_thread[i]->offset = offset;
        runs_per_thread[i]->length = num_runs / NUM_THREADS;
        runs_per_thread[i]->runs = runs;
        if(remainder) {
            runs_per_thread[i]->length++;
            remainder--;
        }
        offset += runs_per_thread[i]->length;
    }
    // Allocate each worker a subset of the runs to sort
    pthread_t workers[NUM_THREADS];
    for(int i=0; i<NUM_THREADS; i++) {
        pthread_create(&workers[i], NULL, qsort_worker, 
                runs_per_thread[i]);
    }
    for(int i=0; i<NUM_THREADS; i++) {
        pthread_join(workers[i], NULL);
    }
    // Free worker data
    for(int i=0; i<NUM_THREADS; i++) {
        free(runs_per_thread[i]);
    }
    free(runs_per_thread);
}


static void* qsort_worker(void *worker_ptr)
{
    worker_data_t* data = (worker_data_t*)worker_ptr;
    
    for(int i=data->offset; i<data->offset+data->length; i++) {
        ext_qsort(data->runs[i]);
    }
    return NULL;
}



static void qsort_recurse(run_t* data_ptr, int left, int right)
{
    if(right > left)
    {
        if(right-left < 5) {
            return bubble(data_ptr, left, right);
        }
        int pivot_index = (right+left)/2; // median index as pivot
        int new_pivot_index = partition(data_ptr, left, right, pivot_index);
        if(new_pivot_index-1-left < right-new_pivot_index) {
            // left half
            qsort_recurse(data_ptr, left, new_pivot_index-1);
            // right half
            return qsort_recurse(data_ptr, new_pivot_index, right);
        } else {
            // right half
            qsort_recurse(data_ptr, new_pivot_index, right);
            // left half
            return qsort_recurse(data_ptr, left, new_pivot_index-1);
        }
    }
}

static int partition(run_t* data_ptr, int left, int right, int pivot_index)
{
    int pivot_value = data_ptr->items[pivot_index]; 

    // swap pivot with rightmost (swap it back later)
    swap(data_ptr, pivot_index, right);

    int store_index = left;

    int i;
    for(i=left; i<right; i++) {
        if(data_ptr->items[i] <= pivot_value) {
            swap(data_ptr, store_index, i);
            store_index++;
        }
        for(int i=0; i<data_ptr->length; i++) {
        }
    }
    swap(data_ptr, store_index, right);
    return store_index;
}

static void swap(run_t* data_ptr, int first, int second)
{
    int temp = data_ptr->items[first];
    data_ptr->items[first] = data_ptr->items[second];
    data_ptr->items[second] = temp;
}

static void bubble(run_t* data_ptr, int left, int right)
{
    int i,j;
    if(right == left) {
        return;
    }

    for(i=left; i<=right; i++) {
        for(j=left; j<=right-1-(i-left); j++) {
            if(data_ptr->items[j] > data_ptr->items[j+1]) {
                swap(data_ptr, j, j+1);
            }
        }
    }
}
