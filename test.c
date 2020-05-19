#include "types.h"
#include "stat.h"
#include "user.h"


void handler1(int signum) {
    printf(1, "handler 1, signum: %d\n", signum);
    while (1) {
        printf(1, "handler 1 alive\n");
        sleep(5);
    }
}

void handler2_and_exit(int signum) {
    printf(1, "handler 2, signum: %d, now exiting...\n", signum);
    exit();
}

void kill_while_sleep() {
    printf(1, "kill_while_sleep\n");
    uint childpid;
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        sleep(100000);
    } else {
        sleep(100);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void send_sig_stop_and_stop_printing() {
    printf(1, "send_sig_stop_and_stop_printing\n");
    uint childpid;
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(20);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send stop\n");
        kill(childpid, SIGSTOP);

        sleep(100);

        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void send_sig_stop_and_stop_printing_cont_and_continue() {
    printf(1, "send_sig_stop_and_stop_printing_cont_and_continue\n");
    uint childpid;
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(20);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send stop\n");
        kill(childpid, SIGSTOP);

        sleep(100);

        printf(1, "father: send cont (now should print)\n");
        kill(childpid, SIGCONT);
        sleep(20);

        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void sleeping_and_send_sig_stop() {
    printf(1, "sleeping_and_send_sig_stop\n");
    uint childpid;
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        sleep(10000000);
    } else {
        sleep(20);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send stop\n");
        kill(childpid, SIGSTOP);

        sleep(100);

        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void test_block_sigcont() {
    printf(1, "test_block_sigcont\n");
    uint childpid;
    sigprocmask(sigcontpow);
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(10);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send stop\n");
        kill(childpid, SIGSTOP);

        sleep(100);
        printf(1, "father: send cont(blocked)\n");
        kill(childpid, SIGCONT);
        sleep(50);
        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void test_ignore_sigcont() {
    printf(1, "test_ignore_sigcont\n");
    uint childpid;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (void *) SIG_IGN;
    sa.sigmask = 0;

    sigaction(SIGCONT, &sa, null);
    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(10);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send stop\n");
        kill(childpid, SIGSTOP);

        sleep(100);
        printf(1, "father: send cont(ignored)\n");
        kill(childpid, SIGCONT);
        sleep(50);
        printf(1, "father: send kill\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void test_user_signal() {
    printf(1, "test_user_signal\n");
    uint childpid;
    struct sigaction sa;
    sa.sigmask = 0;
    sa.sa_handler = &handler2_and_exit;
    sigaction(2, &sa, null);

    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            sleep(5);
        }
    } else {
        sleep(10);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send user signal 2\n");
        kill(childpid, 2);
        wait();
    }
}

void test_user_signal_blocked() {
    printf(1, "test_user_signal_blocked\n");
    uint childpid;
    struct sigaction sa;
    sa.sigmask = 0;
    sa.sa_handler = &handler2_and_exit;
    sigaction(2, &sa, null);

    sigprocmask(4);

    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            sleep(5);
        }
    } else {
        sleep(10);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send user signal 2 (blocked)\n");
        kill(childpid, 2);
        sleep(100);
        printf(1, "father: killing child\n");
        kill(childpid, SIGKILL);
//        wait();
    }
}



int main(int argc, char *argv[]) {
//    kill_while_sleep();
//    printf(1, "\n");
//    send_sig_stop_and_stop_printing();
//    printf(1, "\n");
//    sleeping_and_send_sig_stop();
//    printf(1, "\n");
//    send_sig_stop_and_stop_printing_cont_and_continue();
//    printf(1, "\n");
//    test_block_sigcont();
//    printf(1, "\n");
//    test_ignore_sigcont();
//    printf(1, "\n");
//    test_user_signal();
    printf(1, "\n");
    test_user_signal_blocked();

    exit();
}

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
