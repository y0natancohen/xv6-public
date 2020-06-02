typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define MAX_PSYC_PAGES 16
#define MAX_TOTAL_PAGES 32
#define MAX_SWAP_PAGES (MAX_TOTAL_PAGES - MAX_PSYC_PAGES)


#define QUEUE_ELEMENTS 16
#define QUEUE_SIZE (QUEUE_ELEMENTS + 1)

struct queue
{
    int Queue[QUEUE_SIZE];
    int QueueIn, QueueOut;
};
