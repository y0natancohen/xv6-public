#include "types.h"
#include "stat.h"
#include "user.h"
//#include "proc.h"
#define SIGKILL 9

int
main(int argc, char **argv)
{
  int i;

  if(argc < 2){
    printf(2, "usage: kill pid...\n");
    exit();
  }
  for(i=1; i<argc; i++)
    kill(atoi(argv[i]), SIGKILL);
  exit();
}
