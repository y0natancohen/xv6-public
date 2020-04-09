#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int priority = atoi(argv[1]);
  printf(1, "user gave policy num: %d\n",priority);
  int policyCallRes = policy(priority);
  printf(1, "policy syscall returned: %d\n",policyCallRes);
  if(policyCallRes != -1){
    if(priority==0){  
      printf(1, "Policy has successfully changed to Default Policy\n");
    }else if(priority==1){
      printf(1, "Policy has successfully changed to Priority Policy\n");
    }else {
      printf(1, "Policy has successfully changed to CFS Policy\n");
    }
  }else{
    printf(1, "Error replacing policy, no such a policy number (%d)\n", priority);
  }
  exit(0);
}
