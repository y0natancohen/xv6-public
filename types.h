typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef uint pde_t;

#define SIGNALS_SIZE 32

#define SIG_DFL 0 /* default signal handler */
#define SIG_IGN 1 /* ignore signal */
#define SIGKILL 9
//#define SIGKILL 1
#define SIGSTOP 17
#define SIGCONT 19