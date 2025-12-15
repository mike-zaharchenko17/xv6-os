#include "types.h"
#include "user.h"
#include "uthreads.h"

static sem_t s;

static void *waiter(void *arg) {
    printf(1, "waiter: before wait\n");
    sem_wait(&s);
    printf(1, "waiter: after wait\n");
    return 0;
}

/*

Expected order:

created waiter tid=1
waiter: before wait
main: posting
waiter: after wait
semaphore_basic_test: PASS

*/

int main(void) {
    thread_init();
    sem_init(&s, 0);

    int tid = thread_create(waiter, 0);
    printf(1, "created waiter tid=%d\n", tid);

    // let waiter run and block in sem_wait
    thread_yield();

    printf(1, "main: posting\n");
    sem_post(&s);

    // give waiter a chance to wake and print
    thread_yield();

    printf(1, "semaphore_basic_test: PASS\n");
    exit();
}