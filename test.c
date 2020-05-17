#include "types.h"
#include "stat.h"
#include "user.h"

// #define	SIGKILL	9
// #define	SIGSTOP	17
// #define	SIGCONT	19

// int cchildpid[5];
// int j;

// void sigcatcher(int signum){
//     printf("childPID %d caught process. signal number: %d \n",getchildpid());
//     if (j > -1){
//         kill(cchildpid[j],SIGKILL);
//     }
// }

void handler(int sig)
{
    printf(1, "handler #1 on user space, signum: %d\n", sig);
    while(1==1){
            printf(1, "handler 1\n");
        }
}

void handler2(int sig)
{
    printf(1, "handler #2 on user space, signum: %d\n", sig);
}


void test5(){
    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        sleep(10000);
    }
    else
    {
        printf(1,"father: pid: %d\n",getpid());
        sleep(5);
        kill(childpid, SIGKILL); //like kill 9 in default
        wait();
    }

    // restoreActions();
    // passedTest(5);
}
int main(int argc, char *argv[])
{
    uint s = 10;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler;
    sa.sigmask = 524288;
    sigaction(2, &sa, null);
    //sa.sa_handler = &handler;

    // trying to override SIGSTOP default handler - forbidden!
    // int res = sigaction(SIGSTOP, &sa, null);
    // printf(1, "syscall sigaction res: %d\n", res);

    // trying to override SIGKILL default handler - forbidden!
    // int res = sigaction(SIGKILL, &sa, null);
    // printf(1, "syscall sigaction res: %d\n", res);

    // trying to block SIGSTOP signal - forbidden!
    // uint oldMask = sigprocmask(512); 
    // printf(1, "old mask is: %d\n", oldMask);
    
    // trying to block SIGKILL signal - forbidden!
    // uint oldMask = sigprocmask(131072); 
    // printf(1, "old mask is: %d\n", oldMask);

    // trying to ignore SIGCONT signal - allowed!
    // int res = sigaction(SIGCONT, &sa, null);
    // printf(1, "syscall sigaction res: %d\n", res);

    // trying to block SIGCONT signal - allowed!
    // printf(1, "blocking cont sig\n");
    
    // sigprocmask(524288); // block sigcont 
    // sigprocmask(4); // block user
    // printf(1, "old mask is: %d\n", oldMask);

    // assign sig number=2 signal to user handler
    // sigaction(SIGCONT, &sa, null);
    // printf(1, "registered user action to SIGCONT(19)\n");
    // block user handled signal
    // sigprocmask(4);

    // sigaction(3, &sa, null);
    // sa.sa_handler = &handler;
    // sigaction(4, &sa, null);
    // sa.sa_handler = &handler2;
    // sigaction(5, &sa, null);
    //kill(getchildpid(),4);
    //kill(getchildpid(),5);

    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child pid: %d\n",getpid());
        // while(1==1){
        // }
        sleep(1000000);
    }
    else
    {
        // printf(1,"father pid: %d\n",getpid());
        // sleep(s);
        // kill(childpid, 2);
        // printf(1,"father: send user \n");

        // sleep(s);
        // printf(1,"father: send stop \n");
        // kill(childpid,SIGSTOP);

        // sleep(s*5);
        // kill(childpid, SIGCONT);
        // printf(1,"father: send sig cont \n");

        sleep(s*5);
        printf(1,"father: send kill\n");
        kill(childpid, SIGKILL);
        // wait();
    }
    
    exit();
}
