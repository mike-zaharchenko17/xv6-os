#include "uthreads_test_suite.h"

static sem_t s;

/* basic semaphore test */

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
        printf(1, "[FAIL] semaphore_basic_test_ok: got %c%c%c%c\n",
               buf[0], buf[1], buf[2], buf[3]);
        exit();
    }
}

int main(void) {
    printf(1, "=== semaphore suite ====\n");

    run_ok("semaphore basic test: correctly modifies buffer", semaphore_basic_test_ok);

    printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
    exit();
}
