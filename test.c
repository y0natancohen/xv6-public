#include "types.h"
#include "stat.h"
#include "user.h"

void fork_cow_with_swap() {
    int pages = 16;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'a';
    }
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

    } else {
        for (int i = 0; i < pages; i++) {
            printf(1, "father data: %c\n", buf[i * 4096]);
        }
        wait();
    }
}

void fork_cow_no_swap() {
    int pages = 10;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'a';
    }
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

    } else {
        for (int i = 0; i < pages; i++) {
            printf(1, "father data: %c\n", buf[i * 4096]);
        }
        wait();
        printf(1, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFballoc\n");
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
    int pages = 25;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(4096 * pages);
    for (int i = 0; i < pages; i++) {
        buf[i * 4096] = 'a';
    }
    printf(1, "good\n");
    for (int i = 0; i < pages; i++) {
        printf(1, "data: %c\n", buf[i * 4096]);
    }

    printf(1, "calling free\n");
    // sleep(10);
    // free(buf);
}

int main(int argc, char *argv[]) {
     fork_cow_no_swap();
//    swap_no_fork();
    // simple_fork();
//     fork_cow_with_swap();
    exit();
}
