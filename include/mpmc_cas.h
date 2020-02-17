#ifndef MPMC_CAS_H
#define MPMC_CAS_H

#include "fifo.h"

/* maximum threads that can be spawned on this system (e.g., # of cores) */
#define MAX_THREADS         20

/* number of entry_t type elements in the ring-buffer of queue */
#define QUEUE_SIZE          1024
#define QUEUE_MASK          (QUEUE_SIZE - 1)

/* number of producers and consumers */
#define NUM_PRODUCER        2
#define NUM_CONSUMER        2    


/* status of a given queue entry */
typedef enum status{
    NILL=0,
    FULL,
    BUSY
}status_t;

/* a single slot of the queue is defined as entry_t */
typedef struct ALIGNED entry {
    volatile status_t flag;                 // indicates the current status of this entry 
    volatile UINT32 data;                   // actual data item in the entry
}entry_t;

/* queue data structure */
typedef struct ALIGNED queue {
    volatile UINT32 head        ALIGNED;    // pointer to head of the queue
    volatile UINT32 tail        ALIGNED;    // pointer to the tail of the queue
    entry_t *buffer             ALIGNED;    // pointer to the associated data buffer
} queue_t;


/* queue initialiazation */
void queue_init (queue_t *q, const UINT64 size)
{
    q->buffer = aligned_alloc(PAGE_SIZE, sizeof(entry_t) * size);
    assert(q->buffer);
    memset(q->buffer, 0, sizeof(entry_t) * size);
}


/* queue finalization */
void queue_finalize (queue_t *q)
{
    if (!q->buffer)
        free(q->buffer);

    if (!q) 
        free (q);
}


/* enqueue operation */
inline int enqueue (queue_t *q, pthread_wrapper_t *thread, UINT32 elem)
{
    /* atomically increment the tail */
    UINT32 tail = __sync_fetch_and_add(&q->tail, 1) & QUEUE_MASK;

    /* fetch entry from the tail of the queue's buffer */
    entry_t *entry = &q->buffer[tail];

    /* atomically CAS check the status of the entry and wait until it becomes available. 
     * Mark the entry as busy as soon as it becomes available */
    while(!__sync_bool_compare_and_swap(&entry->flag, NILL, BUSY));

    /* We are safe to update this entry after marking it busy. 
     * Copy the data item in entry and mark the flag to indicate 
     * that there is data present now */
    entry->data = elem;
    entry->flag = FULL;

    // on weak-ordering architectures, a memory fence should follow 
    // above two write. However, for x86_64 arch we should be good for now

    return SUCCESS;
}

/* dequeue operation */
inline int dequeue (queue_t *q, pthread_wrapper_t *thread, UINT32 *elem)
{
    /* atomically increment the head of the queue */
    UINT32 head = __sync_fetch_and_add(&q->head, 1) & QUEUE_MASK;

    /* fetch the reference to the entry at the head */
    entry_t *entry = &q->buffer[head];

    /* atomically CAS check the status of the entry and wait until producer 
     * has placed the data. Mark the entry as busy */
    while(!__sync_bool_compare_and_swap(&entry->flag, FULL, BUSY));

    /* We are safe to update this entry after marking it busy.
     * Copy the data from the entry and mark the flag to indicate 
     * that the data has been pop'ed and entry is available again.
     */

    *elem = entry->data;
    entry->flag = NILL;
    
    // on weak-ordering architectures, a memory fence should follow 
    // above two write. However, for x86_64 arch we should be good for now

    return SUCCESS;
}

#endif
