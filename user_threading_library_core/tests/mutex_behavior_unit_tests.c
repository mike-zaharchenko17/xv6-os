#include "uthreads_test_suite.h"

static mutex_t m;

/* execution paths */

static void mutex_self_lock_should_exit(void) {
    thread_init();
    mutex_init(&m);

    mutex_lock(&m);
    mutex_lock(&m); // should print a deadlock message
}

static void mutex_unlock_not_by_owner_should_exit(void) {
    thread_init();
    mutex_init(&m);

    m.locked = 1;
    m.owner = (struct thread*)0xdeadbeef;
    
    mutex_unlock(&m);
}

static void mutex_unlock_by_owner_should_run(void) {
    thread_init();
    mutex_init(&m);
    mutex_lock(&m);
    mutex_unlock(&m);
}

/* counter setup and test */
static volatile int counter = 0;

static void *counter_worker(void *arg) {
    for (int i = 0; i < 1000; i++) {
        mutex_lock(&m);

        // begin critical section
        int tmp = counter;
        // force a context switch while holding lock
        thread_yield();
        counter = tmp + 1;
        // end critical section

        mutex_unlock(&m);

        // optional: encourage mixing
        if ((i % 50) == 0)
            thread_yield();
    }

    return 0;
}

static void mutex_protects_counter_ok(void) {
    thread_init();
    mutex_init(&m);

    counter = 0;

    int t1 = thread_create(counter_worker, 0);
    int t2 = thread_create(counter_worker, 0);
    int t3 = thread_create(counter_worker, 0);
    int t4 = thread_create(counter_worker, 0);

    thread_join(t1);
    thread_join(t2);
    thread_join(t3);
    thread_join(t4);

    if (counter != 4 * 1000) {
        printf(1, "[FAIL] mutex counter wrong: %d\n", counter);
        exit();
    }
}

static void *counter_worker_no_mutex(void *arg) {
    for (int i = 0; i < 1000; i++) {
        // begin critical section (unprotected)
        int tmp = counter;
        // force a context switch while holding lock
        thread_yield();
        counter = tmp + 1;
        // end unprotected critical section

        // optional: encourage mixing
        if ((i % 50) == 0)
            thread_yield();
    }
    
    return 0;
}

static void unprotected_counter_test_fails(void) {
    thread_init();
    mutex_init(&m);

    int expected = 4000;
    counter = 0;

    int t1 = thread_create(counter_worker_no_mutex, 0);
    int t2 = thread_create(counter_worker_no_mutex, 0);
    int t3 = thread_create(counter_worker_no_mutex, 0);
    int t4 = thread_create(counter_worker_no_mutex, 0);

    thread_join(t1);
    thread_join(t2);
    thread_join(t3);
    thread_join(t4);

    if (counter != 4 * 1000) {
        printf(1, "[PASS] mutex counter wrong (expected: %d, actual: %d)\n", expected, counter);
    } else {
        printf(1, "[FAIL] mutex counter correct");
    }

    exit();
}


int main(void) {
    printf(1, "=== mutex suite ====\n");

    run_expect_exit("mutex self lock exits", mutex_self_lock_should_exit);

    run_expect_exit("mutex unlock by not owner exits", mutex_unlock_not_by_owner_should_exit);

    run_ok("mutex lock/unlock by owner runs", mutex_unlock_by_owner_should_run);

    run_ok("mutex correctly protects counter", mutex_protects_counter_ok);

    // run ok because you're not gonna get kicked out
    run_ok("counter test fails without mutex", unprotected_counter_test_fails);

    printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
    exit();
}

