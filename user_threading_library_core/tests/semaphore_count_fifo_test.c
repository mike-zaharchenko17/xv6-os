#include "types.h"
#include "user.h"
#include "uthreads.h"

static sem_t s;

static volatile int order[3];
static volatile int idx = 0;

static void *w1(void *arg) {
    sem_wait(&s);
    order[idx++] = 1;
    return 0;
}
static void *w2(void *arg) {
    sem_wait(&s);
    order[idx++] = 2;
    return 0;
}
static void *w3(void *arg) {
    sem_wait(&s);
    order[idx++] = 3;
    return 0;
}

int main(void) {
    thread_init();
    sem_init(&s, 0);

    int t1 = thread_create(w1, 0);
    int t2 = thread_create(w2, 0);
    int t3 = thread_create(w3, 0);
    printf(1, "created %d %d %d\n", t1, t2, t3);

    // Run them so they all block on sem_wait, in creation order
    thread_yield(); // w1 blocks
    thread_yield(); // w2 blocks
    thread_yield(); // w3 blocks

    // Now release exactly one at a time
    sem_post(&s);
    thread_yield();

    sem_post(&s);
    thread_yield();

    sem_post(&s);
    thread_yield();

    printf(1, "order: %d %d %d\n", order[0], order[1], order[2]);

    if (order[0] != 1 || order[1] != 2 || order[2] != 3) {
        printf(1, "semaphore_fifo_test: FAIL\n");
        exit();
    }

    printf(1, "semaphore_fifo_test: PASS\n");
    exit();
}