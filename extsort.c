#include "extsort.h"

// Number of pages in a run
static unsigned long DATA_SIZE;
static char* DATA_FILENAME;
static unsigned long BUF_SIZE;
static unsigned int IO_BUF_PAGES;

int main(int argc, char* argv[])
{
    if(argc != 4) {
        printf("Invalid number of arguments (%d)\n", argc);
        usage();
    }
    DATA_FILENAME = argv[1];
    BUF_SIZE = atoi(argv[2])*(1<<20); // convert from MB to bytes
    IO_BUF_PAGES = atoi(argv[3]);

    int sort_fd = open(DATA_FILENAME, O_RDONLY);
    if(sort_fd < 0) {
        error("Could not open sort_fd!");
    }

    // Get the data file's size
    struct stat s;
    int rv = fstat(sort_fd, &s);
    if(rv) {
        printf("%s\n", DATA_FILENAME);
        error("Could not fstat file!");
    }
    DATA_SIZE = s.st_size;

    char* filename = (char*)calloc(1, 100);

    // initially, each run is just 1 page, so this is how many runs there are
    int run_length = 1;
    unsigned long num_runs = DATA_SIZE / PAGE_SIZE;
    if(DATA_SIZE % PAGE_SIZE) {
        num_runs++;
    }

    printf("Sorting %lu ints (%.02f MB)...\n", 
            num_runs*INTS_SIZE,
            (double)num_runs*PAGE_SIZE / (1<<20));

    char input_prefix[] = "foo_";
    char output_prefix[] = "bar_";

    while(num_runs > 1) {
        printf("Iterate: %lu runs left\n", num_runs);
        // Number of ways we can merge at once
        unsigned long num_ways = ((BUF_SIZE / PAGE_SIZE) / IO_BUF_PAGES) - 1;
        // Case for small sorts: fits in memory
        if(num_ways > num_runs) {
            num_ways = num_runs;
        }
        // Calculate how many multimerges need to be done to merge all runs
        unsigned long num_merges = num_runs / num_ways;
        if(num_runs % num_ways) {
            num_merges++;
        }

        unsigned long run_counter = 0;

        // Start iterating the multimerges
        for(int i=0; i<num_merges; i++) {
            printf("Merge %d of %lu (%lu ways)\n", i+1, num_merges, num_ways);
            // Normal case: merge n-ways
            unsigned long num_runs_in_merge = num_ways;
            // Remainder case: merge num_runs % num_ways
            if(run_counter >= (num_runs/num_ways)*num_ways) {
                num_runs_in_merge = num_runs % num_ways;
            }

            // Allocate the runs and runfds we're using for this multimerge
            run_t** runs = (run_t**)calloc(num_runs_in_merge, sizeof(run_t*));

            // Handle the first pass differently, since the numbers
            // are all coming out of the same file, we need to populate
            // the runs here instead of letting multimerge do it
            if(run_length == 1) {
                // Allocate different sized runs
                PRINTF("Base case: init and sorting pages\n");
                for(int i=0; i<num_runs_in_merge; i++) {
                    runs[i] = (run_t*)calloc(1, sizeof(run_t));
                    runs[i]->items = (int*)calloc(1, PAGE_SIZE);
                    int bytes = read(sort_fd, runs[i]->items, PAGE_SIZE);
                    runs[i]->length = bytes/sizeof(int);
                    if(bytes == 0) {
                        break;
                    }
                }
                run_counter += num_runs_in_merge;

                PRINTF("Doing parallel qsort...");
                parallel_qsort(runs, num_runs_in_merge);
                PRINTF("done!\n");
                // Verify the quicksort
#ifdef DEBUG
                PRINTF("Verifying qsort results...");
                for(int i=0; i<num_runs_in_merge; i++) {
                    int temp = 0;
                    int init = 0;
                    for(int j=0; j<runs[i]->length; j++) {
                        if(!init) {
                            temp = runs[i]->items[j];
                            init = 1;
                        }

                        if(runs[i]->items[j] < temp) {
                            printf("Incorrect qsort! Run %d idx %d\n",
                                    i, j);
                            exit(-1);
                        }
                        temp = runs[i]->items[j];
                    }
                }
                PRINTF("correct!\n");
#endif
            }
            else {
                for(int i=0; i<num_runs_in_merge; i++) {
                    snprintf(filename, 100, "%s%lu.dat", input_prefix, run_counter);
                    printf("Reading in: %s\n", filename);
                    run_counter++;
                }
            }

            snprintf(filename, 100, "%s%d.dat", output_prefix, i);
            printf("Writing out: %s\n", filename);
            int output_fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 
                    S_IRWXU|S_IRWXG);
            // Merge them together
            int base = 0;
            if(run_length == 1) {
                base = 1;
            }
            multimerge(runs, input_prefix, num_runs_in_merge, output_fd, base);

            close(output_fd);

            // Close and free runs / fds
            // Free the allocated runs if base case
            if(base == 1) {
                for(int i=0; i<num_runs_in_merge; i++) {
                    free(runs[i]->items);
                    free(runs[i]);
                }
            }
            free(runs);

        }
        // Swap the input and output prefixes
        // This way the next merge uses the previous round's output
        // as input
        strcpy(filename, output_prefix);
        strcpy(output_prefix, input_prefix);
        strcpy(input_prefix, filename);

        // RUN_LENGTH increases by N_WAYS every iteration
        run_length *= num_ways;
        // Number of runs produced is one per merge, i.e. num_merges
        num_runs = num_merges;
    }

    printf("Done sorting.\n");
#ifdef DEBUG
    snprintf(filename, 100, "%s%d.dat", input_prefix, 0);
    verify(filename);
#endif

    free(filename);

    return 0;
}


/* multimerge
 *
 * On the first pass, runs are passed already allocated. This is
 * indicated by the boolean <base> parameter
 *
 * Further passes need to allocate IO_BUF_PAGES for each run and read
 * in based on the input_prefix and num_runs. We can't keep the fds
 * open because there might be too many, so reopen lazily.
 *
 * output_fd comes already open and initialized for writing.
 */
void multimerge(run_t** runs, char* input_prefix, int num_runs, int output_fd, int base)
{
    // Allocate run bufs if not base case
    int run_offsets[num_runs];
    char filename[20];
    if(base == 0) {
        PRINTF("multimerge: allocating run bufs\n");
        for(int i=0; i<num_runs; i++) {
            runs[i] = (run_t*)calloc(1, sizeof(run_t));
            runs[i]->items = (int*)calloc(1, IO_BUF_PAGES*PAGE_SIZE);
            snprintf(filename, 20, "%s%d.dat", input_prefix, i);
            int fd = open(filename, O_RDONLY);
            int bytes = read(fd, runs[i]->items, IO_BUF_PAGES*PAGE_SIZE);
            close(fd);

            runs[i]->length = bytes/sizeof(int);
            run_offsets[i] = bytes;
        }
    }

    int idx[num_runs];
    int skip[num_runs];

    int* output = (int*)malloc(IO_BUF_PAGES*PAGE_SIZE);
    int output_idx = 0;

    // This decrements every time a run is fully merged
    int live_runs = num_runs;

    for(int i=0; i<num_runs; i++) {
        idx[i] = 0;
        // Could conceivably be passed null runs (you are a
        // horrible person if you do this).
        if(runs[i]->length > 0) {
            skip[i] = 0;
        } else {
            skip[i] = 1;
            live_runs--;
        }
    }

    // Merge runs together
    while(live_runs > 0) {
        // find min element across runs
        int init = 0;
        int min = -13;
        int min_run = -1;
        for(int j=0; j<num_runs; j++) {
            if(!skip[j]) {
                if(!init || runs[j]->items[idx[j]] < min) {
                    min_run = j;
                    min = runs[j]->items[idx[j]];
                    init = 1;
                }
            }
        }
        // put min into output buffer
        output[output_idx] = runs[min_run]->items[idx[min_run]];
        idx[min_run]++;

        // Read in more of the run if the buf is done
        if(!skip[min_run] && idx[min_run] == runs[min_run]->length) {
            if(base) {
                skip[min_run] = 1;
                live_runs--;
            } else {
                // Reopen min_run's fd at right offset to get next chunk
                snprintf(filename, 20, "%s%d.dat", input_prefix, min_run);
                int fd = open(filename, O_RDONLY);
                lseek(fd, run_offsets[min_run], SEEK_SET);
                int bytes = read(fd, runs[min_run]->items, IO_BUF_PAGES*PAGE_SIZE);
                if(bytes > 0) {
                    idx[min_run] = 0;
                    runs[min_run]->length = bytes / sizeof(int);
                    // skip the run in the future if the file is completely read
                } else {
                    skip[min_run] = 1;
                    live_runs--;
                }
                close(fd);
                run_offsets[min_run] += bytes;
            }
        }
        // Flush the write buffer to disk if necessary
        output_idx++;
        if(output_idx == IO_BUF_PAGES*INTS_SIZE) {
            // Verify the buf first
            int check = output[0];
            for(int j=1; j<IO_BUF_PAGES*INTS_SIZE; j++) {
                if(output[j] < check) {
                    printf("Error at index %d\n", j);
                }
                check = output[j];
            }
            // TODO: double buffer and use aio calls
            int rv = write(output_fd, output, IO_BUF_PAGES*PAGE_SIZE);
            if(rv != IO_BUF_PAGES*PAGE_SIZE) {
                perror("Write size mismatch!\n");
            }
            output_idx = 0;
        }
    }
    // Do a final flush
    int bytes = write(output_fd, output, output_idx*sizeof(int));
    if(bytes != output_idx*sizeof(int)) {
        perror("Write size mismatch!\n");
    }

    // Free run bufs if not base case
    if(base == 0) {
        for(int i=0; i<num_runs; i++) {
            free(runs[i]->items);
            free(runs[i]);
        }
    }
    free(output);
}


void verify(char* filename)
{
    printf("Verifying sort (%s)...\n", filename);
    int input_fd = open(filename, O_RDONLY);
    // Get the data file's size
    struct stat s;
    int rv = fstat(input_fd, &s);
    if(rv) {
        error("Could not fstat file!");
    }
    int buf_size = IO_BUF_PAGES*PAGE_SIZE;
    int num_runs = s.st_size / (buf_size);
    if(s.st_size % buf_size) {
        num_runs++;
    }

    int* buf = (int*)calloc(1, buf_size);
    int current = 0;
    int init = 0;
    int error = 0;
    unsigned long count = 0;
    while(num_runs > 0) {
        int bytes = read(input_fd, buf, buf_size);
        for(int i=0; i<bytes/sizeof(int); i++) {
            if(!init) {
                current = buf[i];
                init = 1;
            }
            if(buf[i] < current) {
                error = 1;
            }
            //fprintf(stderr, "%d\n", current);
            current = buf[i];
            count++;
        }
        num_runs--;
    }
    //fprintf(stderr, "%d\n", current);

    if(error) {
        printf("Incorrect sort of %lu ints.\n", count);
    } else {
        printf("Correctly sorted! %lu ints.\n", count);
    }
    free(buf);
}


void usage()
{
    printf("extsort <filename> <buffer_size_mb> <io_buf_pages>\n");
    printf("\tfilename: filename of file to be sorted\n");
    printf("\tbuffer_size_mb: MBs of buffer to use for sorting\n");
    printf("\tio_buf_pages: number of 4k pages to use for per-run IO buffering\n");
    exit(1);
}


void error(const char* msg)
{
    perror(msg);
    exit(-1);
}

