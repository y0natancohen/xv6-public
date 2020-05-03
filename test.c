#include "types.h"
#include "stat.h"
#include "user.h"

struct sigaction {
  void (*sa_handler)(int);
  uint sigmask; 
};

int
main(int argc, char *argv[])
{
  struct sigaction* sig;
  printf(1, "sizeof(struct sigaction) -> %d\n",sizeof(struct sigaction));
  printf(1, "sizeof(sig) -> %d\n",sizeof(sig));
  exit();
}

