#include "uthreads_test_suite.h"

static sem_t s;

/* 

basic semaphore test 

this is a deterministic order / sequencing with two threads test

it is modified from our original test, which had an expecteed order of:

waiter: before wait
main: posting
waiter: after wait
semaphore_basic_test: PASS

*/

static char buf[5];

static void *basic_waiter(void *arg) {
    buf[1] = 'b';
    sem_wait(&s);
    buf[3] = 'd';
    return 0;
}

static void semaphore_basic_test_ok(void) {
    thread_init();
    sem_init(&s, 0);

    buf[0] = buf[1] = buf[2] = buf[3] = '?';

    //terminator
    buf[4] = 0;

    // write to 0 first
    buf[0] = 'a';

    int tid = thread_create(basic_waiter, 0);

    // let waiter run and block in sem_wait()
    thread_yield();

    buf[2] = 'c';
    sem_post(&s);

    // let waiter wake and write 'd'
    thread_yield();

    thread_join(tid);

    printf(1, "buf: %s\n", buf);   // expect "ab?d" unless you set buf[2]
    // If you set buf[2]='c', expect: "abc d" pattern => "abcd" if you fill [2]
    if (buf[0] != 'a' || buf[1] != 'b' || buf[2] != 'c' || buf[3] != 'd') {
        printf(1, "[FAIL] semaphore_basic_test_ok: got %c%c%c%c\n", buf[0], buf[1], buf[2], buf[3]);
        exit();
    }
}

/* semaphore count fifo test */

static volatile int order[3];
static volatile int idx = 0;

static void *fifo_w1(void *arg) {
    sem_wait(&s);
    order[idx++] = 1;
    return 0;
}

static void *fifo_w2(void *arg) {
    sem_wait(&s);
    order[idx++] = 2;
    return 0;
}

static void *fifo_w3(void *arg) {
    sem_wait(&s);
    order[idx++] = 3;
    return 0;
}

static void semaphore_fifo_test_ok() {
    thread_init();
    sem_init(&s, 0);

    int t1 = thread_create(fifo_w1, 0);
    int t2 = thread_create(fifo_w2, 0);
    int t3 = thread_create(fifo_w3, 0);

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
        printf(1, "[FAIL]: semaphore fifo test (got %d %d %d)\n", order[0], order[1], order[2]);
        exit();
    }
}

int main(void) {
    printf(1, "=== semaphore suite ====\n");

    run_ok("semaphore basic test: correctly modifies buffer", semaphore_basic_test_ok);

    run_ok("semaphore fifo test: correctly orders buffer modifications", semaphore_fifo_test_ok);

    printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
    exit();
}
