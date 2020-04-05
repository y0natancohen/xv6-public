#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
//  int *buf, mem, pid;

//  char stats[255];
  char stat_path[255];
//
//
  int pid = getpid();
  char pids[10];
  
//pid += 1;
  sprintf(stat_path, "/proc/%d/statm", pid);
//  puts(str);
//  printf("The process is using: %dB\n", mem);

//  FILE* status = fopen( "/proc//status", "r" );
//  status
//  printf(1, "The process is using: %dB\n", mem);
//
//  printf(1, "Allocating more memory\n");
//  buf = (int*) malloc(2000);
//
//  printf(1, "The process is using: %dB\n", mem);
//  printf(1, "Freeing memory\n");
//  free(buf);
//  printf(1, "The process is using: %dB\n", mem);
  exit();
}

