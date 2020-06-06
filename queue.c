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

int get_index_of(int x, struct queue* q){
    int pages[MAX_PSYC_PAGES];
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        pages[i] = -1;
    }
    // take all out
    int where_x_is = -1;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = QueueGet(q);
        if (next == x){
            where_x_is = i;
        }
        if (next == -1){
            break;
        }
        pages[i] = next;
    }
    // put all back
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = pages[i];
        if (next == -1){
            break;
        }
        QueuePut(next, q);
    }
    return where_x_is;
}

int size_of_q(struct queue* q){
    int pages[MAX_PSYC_PAGES];
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        pages[i] = -1;
    }
    // take all out
    int counter = 0;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = QueueGet(q);
        if (next == -1){
            break;
        }
        pages[i] = next;
        counter += 1;
    }
    // put all back
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = pages[i];
        if (next == -1){
            break;
        }
        QueuePut(next, q);
    }
    return counter;
}

int is_last(int x, struct queue* q){
    int where_x_is = get_index_of(x, q);
    int size = size_of_q(q);
    if ((where_x_is + 1) == size){
        return 1;
    } else {
        return 0;
    }
}


void QueueMoveBackOne(int x, struct queue* q)
{
//    (q->QueueIn -1 ) % QUEUE_SIZE;
    if(q->QueueIn == q->QueueOut)
    {
        return; /* Queue Empty */
    }
    if (is_last(x, q)){
        return;
    }

    int pages[MAX_PSYC_PAGES];
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        pages[i] = -1;
    }
    // take all out
    int where_x_is = -1;
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        int next = QueueGet(q);
        if (next == x){
            where_x_is = i;
        }
        if (next == -1){
            break;
        } else{
            pages[i] = next;
        }

    }
    // put all back, move x back one spot
    for (int i = 0; i < MAX_PSYC_PAGES; ++i) {
        if (pages[i] == -1){
            break;
        }
        if (i == where_x_is){
            QueuePut(pages[i+1], q);
        } else if (i == (where_x_is + 1)){
            QueuePut(pages[where_x_is], q);
        } else{
            QueuePut(pages[i], q);
        }
    }
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
//    cprintf("q in: %d\n", q->QueueIn);
//    cprintf("q out: %d\n", q->QueueOut);
    int i = q->QueueOut;
    while (i != q->QueueIn){
        // cprintf("%d,", q->Queue[i]);
        i = (i +1) % QUEUE_SIZE;
    }
    // cprintf("\n");

}

void QueueCopy(struct queue* from_q, struct queue* to_q){
    to_q->QueueIn = from_q->QueueIn;
    to_q->QueueOut = from_q->QueueOut;
    for(int i=0; i<QUEUE_SIZE; i++){
        to_q->Queue[i] = from_q->Queue[i];
    }
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
