#include "types.h"
#include "defs.h"


void QueueInit(struct queue* q)
{
    q->QueueIn = q->QueueOut = 0;
}

int QueuePut(int x, struct queue* q)
{
    if(q->QueueIn == (( q->QueueOut - 1 + QUEUE_SIZE) % QUEUE_SIZE))
    {
        return -1; /* Queue Full*/
    }

    q->Queue[q->QueueIn] = x;

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

void QueuePrint(struct queue* q)
{
    return;
    cprintf("q in: %d\n", q->QueueIn);
    cprintf("q out: %d\n", q->QueueOut);
    int i = q->QueueOut;
    while (i != q->QueueIn){
        cprintf("%d,", q->Queue[i]);
        i = (i +1) % QUEUE_SIZE;
    }
    cprintf("\n");

}

void QueueRemove(struct queue* q, int x)
{
    int pages[MAX_PSYC_PAGES];
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        pages[i] = -1;
    }
    // take all out
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = QueueGet(q);
        if (next == -1){
            break;
        } else{
            pages[i] = next;
        }

    }
    // put all but x back in
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = pages[i];
        if (next == -1){
            break;
        } else{
            if (next != x){
                QueuePut(next, q);
            }
        }
    }
}
