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
}

void handler2(int sig)
{
    printf(1, "handler #2 on user space, signum: %d\n", sig);
}



int main(int argc, char *argv[])
{
    uint s = 20;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *)SIGCONT;
    sigaction(3, &sa, null);
    sa.sa_handler = &handler;
    sigaction(4, &sa, null);
    sa.sa_handler = &handler2;
    sigaction(5, &sa, null);
    //kill(getchildpid(),4);
    //kill(getchildpid(),5);

    uint childpid;
    if ((childpid = fork()) == 0)
    {
        printf(1,"child: pid: %d\n",getpid());
        printf(1,"child: stop\n");
//        kill(getpid(),SIGSTOP);
        printf(1,"child: continue\n");
        while(1==1){
            printf(1, "alive\n");
        }
    }
    else
    {
        sleep(s);
        kill(childpid,SIGSTOP);
        printf(1,"father: send stop \n");
        sleep(s*3);
        kill(childpid, SIGCONT);
        printf(1,"father: send cont to son\n");
        sleep(s);
        //kill(childpid,3);//custom sigaction to sigcont
        //sleep(10);
//        kill(childpid,SIGSTOP);// not suppose to do anythibg
//        printf(1,"father: send stop \n");

//        sleep(10);
        printf(1,"send 2 user signals to son:\n");

        kill(childpid, 4);
        kill(childpid, 5);

//        kill(childpid, 11); //like kill 9 in default
        sleep(s);
        kill(childpid, SIGKILL);
        wait();
    }


//    printf(1,"bye!!!!!!!!!!!!!!!\n");
    exit();
}
