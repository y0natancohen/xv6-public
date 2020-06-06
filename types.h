//#include "mmu.h"
//#include "memlayout.h"

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define MAX_PSYC_PAGES 16
#define MAX_TOTAL_PAGES 32
#define MAX_SWAP_PAGES (MAX_TOTAL_PAGES - MAX_PSYC_PAGES)

#ifndef NONE
#define QUEUE_ELEMENTS 16
#endif
#ifdef NONE
#define QUEUE_ELEMENTS 200
#endif

#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)

#define MAX_PAGES (PHYSTOP / PGSIZE)

#define LEFT_MOST_BIT (1 << 31)
#define ALL_ONES 0xFFFFFFFF

struct queue
{
    int Queue[QUEUE_SIZE];
    int QueueIn, QueueOut;
};
#ifdef VERBOSE_PRINT
#define VEBOSE_PRINT_VAL 0
#endif

#ifndef VERBOSE_PRINT
#define VEBOSE_PRINT_VAL 1
#endif
