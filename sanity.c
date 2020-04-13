#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2, pid3; 
  int i=100000;
  printf(1, "PID  PS_PRIORITY  STIME  RETIME  RTIME\n");
    // variable pid will store the 
    // value returned from fork() system call 
    pid1 = fork(); 
    // If fork() returns zero then it 
    // means it is child process. 
    if (pid1 == 0) { 
        set_cfs_priority(3);
        set_ps_priority(10);
        // First child needs to be printed 
        // later hence this process is made 
        // to sleep for 3 seconds. 
        // float dummy=0;
        // while(i--){
        //   dummy+=0.1;
        // }
        for(double j = 0;j < i; j+=0.01){
          double x =  x + 3.14 * 89.64;    
        }
        // This is first child process 
        // getpid() gives the process 
        // id and getppid() gives the 
        // parent id of that process. 
        struct perf perf1;
        proc_info(&perf1);
        int child1id = getpid();
        printf(1,"%d       %d       %d       %d       %d\n", child1id, perf1.ps_priority, perf1.stime,perf1.retime,perf1.rtime);
        exit(1);
    } 
    else { 
        pid2 = fork(); 
        if (pid2 == 0) {
          // This is second child which is 
          set_cfs_priority(2);
          set_ps_priority(5); 
          // float dummy=0;
          // while(i--){
          //   dummy+=0.1;
          // }
          for(double j = 0;j < i; j+=0.01){
           double x =  x + 3.14 * 89.64;    
          }
          struct perf perf2; 
          // sleep(2);
          proc_info(&perf2);
          int child2id = getpid();
          // sleep(10);
          printf(1,"%d       %d       %d       %d       %d\n", child2id, perf2.ps_priority, perf2.stime,perf2.retime,perf2.rtime);
          exit(2);
        } 
        else { 
            pid3 = fork(); 
            if (pid3 == 0) { 
                // This is third child which is  
                set_cfs_priority(1);
                set_ps_priority(1);
                // float dummy=0;
                // while(i--){
                //   dummy+=0.1;
                // }
                for(double j = 0;j < i; j+=0.01){
                  double x =  x + 3.14 * 89.64;    
                }
                struct perf perf3;
                proc_info(&perf3);
                int child3id = getpid();
                printf(1,"%d       %d       %d       %d       %d\n", child3id, perf3.ps_priority, perf3.stime,perf3.retime,perf3.rtime);
                exit(3);
            } 
            // If value returned from fork() 
            // in not zero and >0 that means 
            // this is parent process. 
            else { 
                // This is asked to be printed at last 
                // hence made to sleep for 3 seconds. 
                int status1, status2, status3;
                wait(&status1);
                wait(&status2);
                wait(&status3);  
            } 
        } 
    }
  exit(0);
}

