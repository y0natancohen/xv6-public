#include "types.h"
#include "stat.h"
#include "user.h"

void fork_test(){
  if (fork() == 0){ 
    printf(1,"Child..\n"); 
  }
  else{
    printf(1,"Parent..\n"); 
    wait();
  }
}

void test1(){
  int pages = 19;
  // printf(1, "asking for %d pages\n",pages);
  malloc(4096*pages);
  char* buf = malloc(4096*pages);

  for(int i=0; i<pages;i++){
    buf[i*4096] = 'a';
  }
  printf(1, "good\n");
  for(int i=0; i<pages;i++){
    printf(1, "data: %c\n",buf[i*4096]);
  }
}
int main(int argc, char *argv[]) {
  // fork_test();
  test1();
  exit();
}
