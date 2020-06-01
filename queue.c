#include "types.h"
#include "defs.h"


void QueueInit(struct queue* q)
{
    q->QueueIn = q->QueueOut = 0;
}

int QueuePut(int new, struct queue* q)
{
    if(q->QueueIn == (( q->QueueOut - 1 + QUEUE_SIZE) % QUEUE_SIZE))
    {
        return -1; /* Queue Full*/
    }

    q->Queue[q->QueueIn] = new;

    q->QueueIn = (q->QueueIn + 1) % QUEUE_SIZE;

    return 0; // No errors
}

int QueueGet(struct queue* q)
{
    if(q->QueueIn == q->QueueOut)
    {
        return -1; /* Queue Empty - nothing to get*/
    }

    int res = q->Queue[q->QueueOut];
    // *old = q->Queue[q->QueueOut];

    q->QueueOut = (q->QueueOut + 1) % QUEUE_SIZE;

    return res; // No errors
    // return 0; // No errors
}