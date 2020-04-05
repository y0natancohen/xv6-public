#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1, "Hello World Xv6\n");
  exit(0);
  // test for ex.3 ass1
  // int status;
  //   if (fork() == 0)
  //       exit(5);
  //   else
  //       wait(&status);
  //   if (status == 5)
  //       printf(1, "Success!\n");
  //   else
  //       printf(1, "Fail :(\n");
  //   printf(1, "Child exit status is %d\n", status);
  // exit(0);
}

