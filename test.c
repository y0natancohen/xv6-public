#include "types.h"
#include "stat.h"
#include "user.h"

void fork_cow_with_swap() {
    int pages = 17;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'a';
    }
    sleep(20);
    printf(1, "--------going to fork!!--------\n");
    sleep(20);
    if (fork() == 0) {
        for (int i = 0; i < pages; i++) {
            printf(1, "child data: %c\n", buf[i * 4096]);
        }

        for (int i = 0; i < pages; i++) {
            buf[i * 4096] = 'b';
        }

        for (int i = 0; i < pages; i++) {
            printf(1, "child data after change: %c\n", buf[i * 4096]);
        }

        printf(1, "child is exiting!!\n");
        exit();
    } else {
        for (int i = 0; i < pages; i++) {
            printf(1, "father data: %c\n", buf[i * 4096]);
        }
        printf(1, "father is waiting!!\n");
        wait();
        printf(1, "father is freeing!!\n");
        free(buf);
    }
}

void fork_cow_no_swap() {
    int pages = 10;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        sleep(1);
        buf[i * 4096] = 'a';
    }
    if (fork() == 0) {
        for (int i = 0; i < pages; i++) {
            sleep(1);
            printf(1, "child data: %c\n", buf[i * 4096]);
        }
        for (int i = 0; i < pages; i++) {
            sleep(1);
            buf[i * 4096] = 'b';
        }
        for (int i = 0; i < pages; i++) {
            sleep(1);
            printf(1, "child data after change: %c\n", buf[i * 4096]);
        }
        printf(1, "child is exiting!!!!\n");
        exit();
    } else {
        for (int i = 0; i < pages; i++) {
            sleep(1);
            printf(1, "father data: %c\n", buf[i * 4096]);
        }
        printf(1, "father is waiting!!\n");
        wait();
        printf(1, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFballoc\n");
        free(buf);
    }
}

void simple_fork(){
    if (fork() == 0) {
        printf(1, "child \n");
    } else {
        printf(1, "father \n");
        wait();
    }

}

void swap_no_fork() {
    int pages = 20;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'a';
    }
    printf(1, "good\n");
    for (int i = 0; i < pages; i++) {
        printf(1, "data: %c\n", buf[i * 4096]);
    }

    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'b';
    }
    printf(1, "good\n");
    for (int i = 0; i < pages; i++) {
        printf(1, "data: %c\n", buf[i * 4096]);
    }

    printf(1, "calling free\n");
     free(buf);
}

void nfu_test() {
    int pages = 19;
    int loops = 2;
    int time = 5;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);

    // set data
    for (int i = 0; i < pages; i++) {
        sleep(time);
        buf[i * 4096] = 'a';
    }
    // access half data
    for (int j = 0; j < loops; ++j) {
        sleep(time);
        printf(1, "loop: %d\n", j);
        for (int i = pages/2; i < pages; i++) {
            printf(1, "data: %c\n", buf[i * 4096]);
        }
    }
    // access all data
    for (int j = 0; j < loops; ++j) {
        sleep(time);
        printf(1, "loop: %d\n", j);
        for (int i = 0; i < pages; i++) {
            printf(1, "data: %c\n", buf[i * 4096]);
        }
    }
    // sleep(10);
    // free(buf);
}
void scfifo_test() {
    int pages = 20;
//    int loops = 3;
    int time = 1;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);

    // set half data
    for (int i = 0; i < pages/2; i++) {
        sleep(time);
        buf[i * 4096] = 'a';
    }
    // set half data again
    for (int i = 0; i < pages/2; i++) {
        sleep(time);
        buf[i * 4096] = 'a';
    }
    // set all data
    for (int i = pages-1; i >=0; i--) {
        sleep(time);
        buf[i * 4096] = 'a';
    }
    // access half data
//    for (int j = 0; j < loops; ++j) {
//        sleep(time);
//        printf(1, "loop: %d\n", j);
//        for (int i = 0; i < pages/2; i++) {
//            printf(1, "data: %c\n", buf[i * 4096]);
//        }
//    }
    // access all data
//    for (int j = 0; j < loops; ++j) {
//        sleep(time);
//        printf(1, "loop: %d\n", j);
//        for (int i = 0; i < pages; i++) {
//            printf(1, "data: %c\n", buf[i * 4096]);
//        }
//    }
    // sleep(10);
    // free(buf);
}


int main(int argc, char *argv[]) {
    printf(1, "\n\n\n\n\n\n\n");
//    swap_no_fork();
    printf(1, "\n\n\n\n\n\n\n");
//    fork_cow_no_swap();
//    nfu_test();
//    scfifo_test();
     fork_cow_with_swap(); // not working
    // simple_fork();
    exit();
}
