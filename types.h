typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;
#define MAX_PSYC_PAGES 16
#define MAX_TOTAL_PAGES 32
#define MAX_SWAP_PAGES (MAX_TOTAL_PAGES - MAX_PSYC_PAGES)
#define PTE_PG 0x200