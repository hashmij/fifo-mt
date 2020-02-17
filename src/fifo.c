#include <fifo.h>

/* include the queue implementation based on locked vs lock-free version */
#ifdef CAS
  #include <mpmc_cas.h>             // lock-free (compare-and-swap) impl
#else 
  #include <mpmc.h>                 // lock-based
#endif

/* ---------------------------------------------------------------------------------------- */
static     queue_t *q   = NULL;     // global shared queue
static int nthreads     = 0;        // number of threads 
/* ---------------------------------------------------------------------------------------- */

/* entry point of each threads */
void *runner(void *args)
{
    pthread_wrapper_t *thread = (pthread_wrapper_t *)args;
    cpu_set_t cur_mask;
    UINT32 data;
    UINT64 *t_diffs = NULL;
    UINT64 t_start, t_stop, t_diff; 
    UINT64 i, j;

    PRINT_DEBUG("tid: %d type: %d started...\n", thread->id, thread->type);

    /* setup affinities */
    CPU_ZERO(&cur_mask);
    CPU_SET(thread->id, &cur_mask);
    if (sched_setaffinity(0, sizeof(cur_mask), &cur_mask) < 0) {
        PRINT_ERROR("Error: thread binding failed");
        return NULL;
    }

    /* create a time differential array to store timestamp of each thread */
    t_diffs = aligned_alloc(PAGE_SIZE, sizeof(UINT64) * nthreads);

    /* sync before benchmarking */
    pthread_barrier_wait(thread->barrier);

    data = thread->id;
    t_diff = 0;
    
    /* --- begin benchmark --- */
    for (j = 0; j < NUM_ITER; j++) {
        pthread_barrier_wait(thread->barrier);
        t_start = read_tsc();
   
        if (thread->type == PRODUCER) {
            for (i = 0; i < NUM_OPS; i++) {
                while ( enqueue(q, thread, data) != 0 );
            } 
        } else {
            for (i = 0; i < NUM_OPS; i++) {
                while ( dequeue(q, thread, &data) != 0 );
            } 
        }

        t_stop = read_tsc();
        t_diff += t_stop - t_start;
    }
    /* --- end benchmark --- */


    t_diffs[thread->id] = t_diff;
    pthread_barrier_wait(thread->barrier);

    /* --- calculate and print stats --- */
    if (thread->id == 0) {
        /* reduce result across all threads and calculate average */
        for (t_diff = 0, i = 0; i < nthreads; i++) {
            t_diff += t_diffs[i];
        }
        t_diff /= nthreads;

        printf("------ printing stats ------\n");
        printf("cycles/op: %ld, time/op(us): %lf, ops/sec: %'.0lf, " 
                "total-ops/sec: %'.0lf\n", t_diff / (NUM_OPS * NUM_ITER),
                1.0e3 * (t_diff / CPU_FREQ) / (NUM_OPS * NUM_ITER),
                1.0e3 * (NUM_OPS * NUM_ITER) / (t_diff / CPU_FREQ),
                1.0e3 * (NUM_OPS * NUM_ITER) / (t_diff / CPU_FREQ) * nthreads);
    }
    
    return NULL;
}   

/* program entry point */
int main (int argc, char *argv[])
{
    error_t err = SUCCESS;
    pthread_wrapper_t *threads = NULL; 
    pthread_barrier_t barrier;

    int np, nc, qs;
    int i;

    /* parse input */
    np = NUM_PRODUCER;
    nc = NUM_CONSUMER;
    qs = QUEUE_SIZE;

    int option;
    while ((option = getopt(argc, argv, "p:c:q:h")) != -1) {
        switch (option) {
             case 'p' : 
                 np = atoi(optarg);
                 break;
             case 'c' : 
                 nc = atoi(optarg);
                 break;
             case 'q' : 
                qs = atoi(optarg);
                 break;
             case 'h' : 
                 print_usage();
                 exit(1);
                 break;
        }
    }

    /* make sure there are no negatives */ 
    assert (qs > 1 && np > 0 && nc >0);

    /* NOTE: for the sake of simplicity, producers and consumers both issue 
     * same number of enqueue/dequeue operations. The consequence of this is 
     * that the count of producer and consumer threads have to be same. Otherwise, 
     * we will run into an issue where there is either not enough producers 
     * to produce, so consumers will block resulting in a `livelock' and 
     * vice versa. This can be handled by making sure that producers and 
     * consumers issue respective operations accordingly, but in real-time
     * scenarios, we probably would want threads to block.
     *
     */ 
    assert (np == nc);

    /* calculate the total number of threads if user has given any input */
    nthreads = np + nc;
    assert (nthreads <= MAX_THREADS);

    setlocale(LC_NUMERIC, "");
    printf ("Total threads: %d, Producers: %d, Consumers: %d, Queue Size: %d\n", 
            nthreads, np, nc, qs);
  
    if ((err = pthread_barrier_init(&barrier, NULL, nthreads)) != 0) {
        PRINT_ERROR("pthread_barrier_init failed");
        goto cleanup;
    }
    

    /* instantiate threads object */
    threads = malloc (sizeof(pthread_wrapper_t) * nthreads);

    /* initialize the queue and associate ring-buffer */
    q = aligned_alloc(PAGE_SIZE, sizeof(queue_t));
    assert(q);
    memset(q, 0, sizeof(queue_t));
    queue_init(q, qs);

    /* create producer and consumer threads */
    for (i = 0; i < nthreads; i++) {
        threads[i].id = i;
        threads[i].type = (i < np) ? PRODUCER : CONSUMER;
        threads[i].barrier = &barrier;
        if ((err = pthread_create(&threads[i].instance, NULL, runner, (void *)&threads[i])) != 0) {
            PRINT_ERROR("pthread_create failed");
            goto cleanup;
        }
    }
  
    /* wait for completion */ 
    for (i = 0; i < nthreads; i++) {
        if ((err = pthread_join(threads[i].instance, NULL)) != 0) {
            PRINT_ERROR("pthread_join failed");
            goto cleanup;
        }
    } 


cleanup:
    queue_finalize(q);
    if (!threads)
        free(threads);

    return err;
}

