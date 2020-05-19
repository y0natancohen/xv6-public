#include "types.h"
#include "stat.h"
#include "user.h"


void handler1(int signum) {
    printf(1, "handler 1, signum: %d\n", signum);
    while (1) {
        printf(1, "handler 1 alive\n");
        sleep(10);
    }
}

void handler2(int signum) {
    printf(1, "handler 2, signum: %d\n", signum);
}

void handler3_and_exit(int signum) {
    printf(1, "handler 3, signum: %d, now exiting...\n", signum);
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
    sa.sa_handler = &handler3_and_exit;
    sigaction(3, &sa, null);

    sa.sa_handler = &handler2;
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
        printf(1, "father: send user signal 3 and exit\n");
        kill(childpid, 3);
        wait();
    }
}

void test_sa_mask() {
    printf(1, "test_sa_mask\n");
    uint childpid;
    struct sigaction sa;
    sa.sigmask = 0;

    sa.sigmask = 8;
    sa.sa_handler = &handler1;
    sigaction(1, &sa, null);

    sa.sigmask = 0;
    sa.sa_handler = &handler3_and_exit;
    sigaction(3, &sa, null);

    sigprocmask(8);

    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
//            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(10);

//        printf(1, "father: sending user signal 3 (blocked)\n");
//        kill(childpid, 3);
//        sleep(100);

        printf(1, "father: send user signal 1\n");
        kill(childpid, 1);
        sleep(100);

        printf(1, "father: sending user signal 3 (blocked inside handler)\n");
        kill(childpid, 3);
        sleep(100);

        printf(1, "father: killing child\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void test_mask_backup() {
    printf(1, "test_mask_backup\n");
    uint childpid;
    struct sigaction sa;
    sa.sigmask = 0;

    sa.sigmask = 16;
    sa.sa_handler = &handler2;
    sigaction(2, &sa, null);

    sa.sigmask = 0;
    sa.sa_handler = &handler3_and_exit;
    sigaction(3, &sa, null);

    sigprocmask(8);

    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
//            printf(1, "alive\n");
            sleep(5);
        }
    } else {
        sleep(10);

//        printf(1, "father: sending user signal 3 (blocked)\n");
//        kill(childpid, 3);
//        sleep(100);

        printf(1, "father: send user signal 2\n");
        kill(childpid, 2);
        sleep(20);

        printf(1, "father: send user signal 2 again\n");
        kill(childpid, 2);
        sleep(20);

        printf(1, "father: sending user signal 3 (blocked after backup handler)\n");
        kill(childpid, 3);
        sleep(100);

        printf(1, "father: killing child\n");
        kill(childpid, SIGKILL);
        wait();
    }
}

void test_user_signal_blocked() {
    printf(1, "test_user_signal_blocked\n");
    uint childpid;
    struct sigaction sa;
    sa.sigmask = 0;
    sa.sa_handler = &handler3_and_exit;
    sigaction(3, &sa, null);

    sigprocmask(8);

    if ((childpid = fork()) == 0) {
        // printf(1, "child pid: %d\n", getpid());
        while (1) {
            sleep(5);
        }
    } else {
        sleep(10);
        // printf(1, "father pid: %d\n", getpid());
        printf(1, "father: send user signal 3 (blocked)\n");
        kill(childpid, 3);
        sleep(10);
        printf(1, "father: killing child\n");
        kill(childpid, SIGKILL);
        wait();
    }
}



int main(int argc, char *argv[]) {
    kill_while_sleep();
    printf(1, "\n");
    send_sig_stop_and_stop_printing();
    printf(1, "\n");
    sleeping_and_send_sig_stop();
    printf(1, "\n");
    send_sig_stop_and_stop_printing_cont_and_continue();
    printf(1, "\n");
    test_block_sigcont();
    printf(1, "\n");
    test_ignore_sigcont();
    printf(1, "\n");
    test_user_signal();
    printf(1, "\n");
    test_user_signal_blocked();
    printf(1, "\n");
    test_sa_mask();
    printf(1, "\n");
    test_mask_backup();
    printf(1, "\n");
    exit();
}
