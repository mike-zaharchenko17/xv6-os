#include "uthreads_test_suite.h"

static mutex_t m;

static void mutex_self_lock_should_exit(void) {
    thread_init();
    mutex_init(&m);

    mutex_lock(&m);
    mutex_lock(&m); // should print a deadlock message
}

static void mutex_unlock_owner_should_run(void) {
    thread_init();
    mutex_init(&m);
    mutex_lock(&m);
    mutex_unlock(&m);
}

int main(void) {
    printf(1, "=== mutex suite ====");
    
    run_expect_exit("mutex self lock exits", mutex_self_lock_should_exit);

    run_ok("mutex lock/unlock by owner runs", mutex_unlock_owner_should_run);

    exit();
}

