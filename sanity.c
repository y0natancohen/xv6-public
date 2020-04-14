#include "types.h"
#include "stat.h"
#include "user.h"


void long_job(int pid){
    int i = 20000;
    for (int x=0;x<3;x++) {
        for (double j = 0; j < i; j += 0.01) {
            double x = (x + 3.14 * 89.64) / 234.6543;
        }
        sleep(3);
    }
    struct perf perf1;
    proc_info(&perf1);
    printf(1, "%d       %d       %d       %d       %d\n", pid, perf1.ps_priority, perf1.stime, perf1.retime, perf1.rtime);
    exit(1);
}

int
main(int argc, char *argv[]) {
    printf(1, "PID  PS_PRIORITY  STIME  RETIME  RTIME\n");
    if (fork() == 0) {
        set_cfs_priority(3);
        set_ps_priority(10);
        long_job(getpid());
    } else {
        if (fork() == 0) {
            set_cfs_priority(2);
            set_ps_priority(5);
            long_job(getpid());
        } else {
            if (fork() == 0) {
                set_cfs_priority(1);
                set_ps_priority(1);
                long_job(getpid());
            }
            else {
                int status1;
                wait(&status1);
                wait(&status1);
                wait(&status1);
            }
        }
    }
    exit(0);
}
