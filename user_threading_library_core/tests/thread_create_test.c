#include "types.h"
#include "user.h"
#include "uthreads.h"

static void *worker1(void *arg) {
    for (int i = 0; i < 3; i++) {
        printf(1, "worker1 %d\n", i);
        thread_yield();
    }
    return (void*)0xdeadbeef;
}

static void *worker2(void *arg) {
    for (int i = 0; i < 3; i++) {
        printf(1, "worker2 %d\n", i);
        thread_yield();
    }
    return (void*)0xdeadbeef;
}

static void *worker3(void *arg) {
    for (int i = 0; i < 3; i++) {
        printf(1, "worker3 %d\n", i);
        thread_yield();
    }
    return (void*)0xdeadbeef;
}


int main(void) {
    thread_init();

    int tid_worker1 = thread_create(worker1, 0);
    int tid_worker2 = thread_create(worker2, 0);
    int tid_worker3 = thread_create(worker3, 0);

    printf(1, "created tid_worker1=%d\n", tid_worker1);
    printf(1, "created tid_worker2=%d\n", tid_worker2);
    printf(1, "created tid_worker3=%d\n", tid_worker3);

    // expect roughly interleaving output
    for (int i = 0; i < 3; i++) {
        printf(1, "main %d\n", i);
        thread_yield();
    }

    exit();
}