#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2, pid3; 
  int i=100000;
  printf(1, "PID  PS_PRIORITY  STIME  RETIME  RTIME\n"); 
    pid1 = fork();  
    if (pid1 == 0) { 
      // First child 
        set_cfs_priority(3);
        set_ps_priority(10);
        for(double j = 0;j < i; j+=0.01){
          double x =  x + 3.14 * 89.64;    
        } 
        struct perf perf1;
        proc_info(&perf1);
        printf(1,"%d    %d           %d      %d       %d\n", getpid(), perf1.ps_priority, perf1.stime,perf1.retime,perf1.rtime);
    } 
    else { 
        pid2 = fork(); 
        if (pid2 == 0) {
          // Second child 
          set_cfs_priority(2);
          set_ps_priority(5); 
          for(double j = 0;j < i; j+=0.01){
           double x =  x + 3.14 * 89.64;    
          }
          struct perf perf2; 
          proc_info(&perf2);
          printf(1,"%d    %d            %d      %d       %d\n", getpid(), perf2.ps_priority, perf2.stime,perf2.retime,perf2.rtime);
        } 
        else { 
            pid3 = fork(); 
            if (pid3 == 0) { 
                // Third child  
                set_cfs_priority(1);
                set_ps_priority(1);
                for(double j = 0;j < i; j+=0.01){
                  double x =  x + 3.14 * 89.64;    
                }
                struct perf perf3;
                proc_info(&perf3);
                printf(1,"%d    %d            %d      %d       %d\n", getpid(), perf3.ps_priority, perf3.stime,perf3.retime,perf3.rtime);
            }  
            else { 
                // Papa 
                int status1, status2, status3;
                wait(&status1);
                wait(&status2);
                wait(&status3);  
            } 
        } 
    }
  exit(0);
}

