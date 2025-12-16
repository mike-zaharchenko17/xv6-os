#include "uthreads_test_suite.h"

static mutex_t m;

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

int main(void) {
    printf(1, "=== mutex suite ====\n");

    run_expect_exit("mutex self lock exits", mutex_self_lock_should_exit);

    run_expect_exit("mutex unlock by not owner exits", mutex_unlock_not_by_owner_should_exit);

    run_ok("mutex lock/unlock by owner runs", mutex_unlock_by_owner_should_run);

    run_ok("mutex unlock by not owner exits", mutex_unlock_not_by_owner_should_exit);
    printf(1, "=== done: %d run, %d failed, %d\n", tests_run, tests_failed);
    exit();
}

