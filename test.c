#include "types.h"
#include "stat.h"
#include "user.h"
//#include "proc.h"
//#include "signal.h"


void handler(int sig){
    printf(1, "handler #1 on user space, signum: %d\n", sig);
}

void handler2(int sig){
    printf(1, "handler #2 on user space, signum: %d\n", sig);
}

void user_sig_test(void){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handler;
    sigaction(4, &sa, null);
//    sa.sa_handler = &handler2;
//    sigaction(5, &sa, null);

    uint childpid;
    if ((childpid = fork()) == 0){
        printf(1,"child: continue\n");
        while(1){}
    }
    else{
        kill(childpid, 4);
//        kill(childpid, 5);
//        kill(childpid, SIGKILL);
        wait();
    }

    printf(1,"bye!!!!!!!!!!!!!!!\n");

}

int main(int argc, char *argv[]){
    uint childpid;
    if ((childpid = fork()) == 0){
        printf(1,"child: continue\n");
        while(1){
            sleep(5);
            printf(1,"child alive\n");
        }
    }
    else{
        kill(childpid, SIGSTOP);
        printf(1, "stop sent\n");
        sleep(50);
        kill(childpid, SIGCONT);
        printf(1, "cont sent\n");
        sleep(50);

        kill(childpid, SIGSTOP);
        printf(1, "stop sent\n");
        sleep(50);
        kill(childpid, SIGCONT);
        printf(1, "cont sent\n");
        sleep(50);

        printf(1, "finished 2nd\n");
        kill(childpid, SIGSTOP);
        kill(childpid, SIGSTOP);
        kill(childpid, SIGSTOP);

//        wait();
    }

//    printf(1,"bye!!!!!!!!!!!!!!!\n");

    exit();
}
