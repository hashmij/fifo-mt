#ifndef FIFO_H
#define FIFO_H

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <locale.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <malloc.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

/* ---------------------------------------------------------------------------------------- */
/* Global definitions */
/* ---------------------------------------------------------------------------------------- */
#define MAX_THREADS     20          // number of maximum available threads on the system
#define BUF_SIZE        1           // number of elements to be enqueued/dequeued
#define NUM_ITER        10          // number of benchmark iterations
//#define NUM_OPS         1L * 1000 * 1000    // number of enqueue / dequeue operations 
#define NUM_OPS         1L * 1000 * 10    // number of enqueue / dequeue operations 
#define CPU_FREQ        3099776.0   // CPU frequency for low-level timers          

/* ---------------------------------------------------------------------------------------- */
/* debugging and error printing */
/* ---------------------------------------------------------------------------------------- */
# define PRINT_ERROR(args...) printf("[ERROR]: "); printf(args); printf("\n"); fflush(stderr);
#ifdef _DEBUG
  # define PRINT_DEBUG(args...) printf("[DEBUG]: "); printf(args); printf("\n"); fflush(stdout);
#else
  # define PRINT_DEBUG(args...)
#endif

/* usage message */
const char usage[] = "usage: ./a.out "
                      "-p [num producers] "
                      "-c [num consumers] "
                      "-q [queue size] "
                      "-h [print usage]";

inline void print_usage() {
    printf("%s\n", usage);
}

/* ---------------------------------------------------------------------------------------- */
/* memory alignment */
/* ---------------------------------------------------------------------------------------- */

#define CACHE_LINE_SIZE             64
#define PAGE_SIZE                   4096

#ifndef ALIGNED
#  if __GNUC__
#    define ALIGNED __attribute__ ((aligned (CACHE_LINE_SIZE)))
#  else
#    define ALIGNED
#  endif
#endif


/* ---------------------------------------------------------------------------------------- */
/* global types and enums */
/* ---------------------------------------------------------------------------------------- */

typedef uint64_t                    UINT64;
typedef uint32_t                    UINT32;
typedef uint16_t                    UINT16;

typedef enum error {
    SUCCESS=0,
    FAILURE
} error_t;

typedef enum thread_type {
    PRODUCER=0,
    CONSUMER
} thread_type_t;

typedef ALIGNED struct pthread_wrapper {
    pthread_t           instance;       // instance of actual pthread object 
    thread_type_t       type;           // type of thread e.g., Producer, Consumer
    UINT32              id;             // identifier
    pthread_barrier_t   *barrier;       // reference to the common barrier 
} pthread_wrapper_t;


/* ---------------------------------------------------------------------------------------- */
/* low-level timers */
/* ---------------------------------------------------------------------------------------- */

#if defined(__x86_64__)
inline UINT64 read_tsc() {
    /* low-level timers are CPU arch specific and thus need to be defined for given 
     * arch-type. For now I am assuming that arch is __x86_64__ and we have rdtsc register 
     * for non-x86_64 archs, just use gettimeofday */

    UINT64        time;
    UINT32        msw   , lsw;
    __asm__         __volatile__("rdtsc\n\t"
                    "movl %%edx, %0\n\t"
                    "movl %%eax, %1\n\t"
                    :         "=r"         (msw), "=r"(lsw)
                    :
                    :         "%edx"      , "%eax");
    time = ((UINT64) msw << 32) | lsw;
    return time;
}
#else 
inline double read_tsc() {
    double time;
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
        perror("gettimeofday");
        abort();
    }
    time = ((double)tv.tv_sec) * 1000000 + tv.tv_usec;
    return time;
}
#endif


#endif  /* define(FIFO_H) */
