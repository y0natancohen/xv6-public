#include "types.h"
#include "stat.h"
#include "user.h"

#define PGSIZE 4096

void fork_cow(int with_swap) {
    int pages;
    if (with_swap){
        printf(1, "--------- starting fork_cow with swap test -----------\n");
        pages = 17;
    } else {
        printf(1, "--------- starting fork_cow no swap test -----------\n");
        pages = 10;
    }
    printf(1, "asking for %d pages\n",pages);
//    char *buf = sbrk(PGSIZE * pages);
    char *buf = malloc(PGSIZE * pages);

    for (int i = 0; i < pages; i++)
        buf[i * PGSIZE] = 'a';

    if (fork() == 0) {
        for (int i = 0; i < pages; i++)
            printf(1, "child data: %c\n", buf[i * PGSIZE]);

        for (int i = 0; i < pages; i++)
            buf[i * PGSIZE] = 'b';

        for (int i = 0; i < pages; i++)
            printf(1, "child data after change: %c\n", buf[i * PGSIZE]);

        printf(1, "child is exiting!!!!\n");
        exit();
    } else {
        sleep(50);
        for (int i = 0; i < pages; i++)
            printf(1, "father data: %c\n", buf[i * PGSIZE]);

        printf(1, "father is waiting!!\n");
        wait();
    }

    if (with_swap) printf(1, "--------- finished fork_cow with swap test -----------\n");
    else printf(1, "--------- finished fork_cow no swap test -----------\n");
}

void simple_fork(){
    printf(1, "--------- starting simple_fork test -----------\n");
    if (fork() == 0) {
        printf(1, "child \n");
        exit();
    } else {
        printf(1, "father \n");
        wait();
    }
    printf(1, "--------- finished simple_fork test -----------\n");
}

void swap_no_fork() {
    printf(1, "--------- starting swap_no_fork test -----------\n");
    int pages = 20;
    // printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(PGSIZE * pages);
    for (int i = 0; i < pages; i++)
        buf[i * PGSIZE] = 'a';

    for (int i = 0; i < pages; i++)
        printf(1, "data: %c\n", buf[i * PGSIZE]);

    printf(1, "--------- finished swap_no_fork test -----------\n");
}

void nfu_test() {
    printf(1, "--------- starting nfu_test -----------\n");
    int pages = 19;
    int loops = 2;
    int time = 5;
    printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(PGSIZE * pages);

    // set data
    for (int i = 0; i < pages; i++) {
        sleep(time);
        buf[i * PGSIZE] = 'a';
    }
    // access half data
    for (int j = 0; j < loops; ++j) {
        sleep(time);
        printf(1, "loop: %d\n", j);
        for (int i = pages/2; i < pages; i++) {
            printf(1, "data: %c\n", buf[i * PGSIZE]);
        }
    }
    // access all data
    for (int j = 0; j < loops; ++j) {
        sleep(time);
        printf(1, "loop: %d\n", j);
        for (int i = 0; i < pages; i++) {
            printf(1, "data: %c\n", buf[i * PGSIZE]);
        }
    }
    printf(1, "--------- finished nfu_test -----------\n");
}
void scfifo_test() {
    printf(1, "--------- starting scfifo_test -----------\n");
    int pages = 20;
//    int loops = 3;
    int time = 1;
    printf(1, "asking for %d pages\n",pages);
    char *buf = malloc(PGSIZE * pages);

    // set half data
    for (int i = 0; i < pages/2; i++) {
        sleep(time);
        buf[i * PGSIZE] = 'a';
    }
    // set half data again
    for (int i = 0; i < pages/2; i++) {
        sleep(time);
        buf[i * PGSIZE] = 'a';
    }
    // set all data
    for (int i = pages-1; i >=0; i--) {
        sleep(time);
        buf[i * PGSIZE] = 'a';
    }

    printf(1, "--------- finished scfifo_test -----------\n");
}

void fork_cow_no_swap(){ fork_cow(0); }
void fork_cow_with_swap(){ fork_cow(1); }


int main(int argc, char *argv[]) {
    if (argc < 2)
        exit();

    if (strcmp(argv[1], "1") == 0)
        swap_no_fork();
    else if (strcmp(argv[1], "2") == 0)
        fork_cow_no_swap();
    else if (strcmp(argv[1], "3") == 0)
        fork_cow_with_swap();
    else if (strcmp(argv[1], "4") == 0)
        nfu_test();
    else if (strcmp(argv[1], "5") == 0)
        scfifo_test();
    else if (strcmp(argv[1], "7") == 0)
        simple_fork();
    else if (strcmp(argv[1], "h") == 0 || strcmp(argv[1], "help") == 0) {
        printf(1, "\n\nplease select one of these tests:\n");
        printf(1, "ass3Tests 1 (swap no fork-cow)\n");
        printf(1, "ass3Tests 2 (fork-cow no swap)\n");
        printf(1, "ass3Tests 3 (fork-cow with swap)\n");
        printf(1, "ass3Tests 4 (nfu test)\n");
        printf(1, "ass3Tests 5 (scfifo test)\n");
        printf(1, "ass3Tests 6 (simple fork test, no malloc)\n");
        printf(1, "\n\n");
    }
    else
        printf(1, "unknown test '%s', use 'ass3Tests help' for usage\n", argv[1]);

    exit();
}
