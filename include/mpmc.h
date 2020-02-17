#ifndef MPMC_H
#define MPMC_H

#include "fifo.h"

/* maximum threads that can be spawned on this system (e.g., # of cores) */
#define MAX_THREADS         20

/* number of entry_t type elements in the ring-buffer of queue */
#define QUEUE_SIZE          1024
#define QUEUE_MASK          (QUEUE_SIZE - 1)

/* number of producers and consumers */
#define NUM_PRODUCER        2
#define NUM_CONSUMER        2    


/* a single slot of the queue is defined as entry_t */
typedef struct ALIGNED entry {
    volatile UINT32 data;
}entry_t;

/* queue data structure */
typedef struct ALIGNED queue {
    volatile UINT32 head        ALIGNED;    // pointer to head of the queue
    volatile UINT32 tail        ALIGNED;    // pointer to the tail of the queue
    volatile UINT32 lock        ALIGNED;    // used to lock/unlock the queue 
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
    /* lock the queue, increment tail, and unlock the queue */
    while(__sync_lock_test_and_set(&q->lock, 1));
    UINT32 tail = q->tail++ & QUEUE_MASK;
    __sync_lock_release(&q->lock);
    
    /* get entry at the tail of queue's buffer and make sure queue is not full */
    entry_t *entry = &q->buffer[tail];
    while(tail == q->head);

    /* add data to the queue entry */
    entry->data = elem;
    
    // on weak-ordering architectures, a memory fence should be placed here.
    // However, for x86_64 arch we should be good for now. 

    return SUCCESS;
}

/* dequeue operation */
inline int dequeue (queue_t *q, pthread_wrapper_t *thread, UINT32 *elem)
{
    /* lock the queue, increment head, and unlock the queue */
    while (__sync_lock_test_and_set(&q->lock, 1));
    UINT32 head = q->head++ & QUEUE_MASK;
    __sync_lock_release(&q->lock);
    
    /* get entry at the tail of queue's buffer and make sure queue is not full */
    entry_t *entry = &q->buffer[head];
    while (head == q->tail);

    /* fetch data from the queue entry */
    *elem = entry->data;

    // on weak-ordering architectures, a memory fence should be placed here.
    // However, for x86_64 arch we should be good for now. 
    
    return SUCCESS;
}

#endif
