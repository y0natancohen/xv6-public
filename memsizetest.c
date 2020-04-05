#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "The process is using: %dB\n", memsize());
  printf(1, "Allocating more memory\n");
  char* mres = malloc(2048);
  printf(1, "The process is using: %dB\n", memsize());
  printf(1, "Freeing memory\n");
  free(mres);
  printf(1, "The process is using: %dB\n", memsize());
  exit(0);
}

