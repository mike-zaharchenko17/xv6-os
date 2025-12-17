#include "uthreads_test_suite.h"

static void thread_init_ok(void) {
    thread_init();

    if (&threads[0] != current_thread) {
        printf(1, "[FAIL] thread at idx 0 and current thread tid did not match\n");
        exit();
    }

    if (current_thread->tid != 0) {
        printf(1, "[FAIL] current thread tid is incorrect\n");
    }

    for (int i = 1; i < MAX_THREADS; i++) {
        if (
            !(threads[i].tid == -1) ||
            !(threads[i].tstate == T_UNUSED) ||
            !(threads[i].stack == 0) ||
            !(threads[i].sp == 0) ||
            !(threads[i].start_routine == 0) ||
            !(threads[i].arg == 0) ||
            !(threads[i].retval == 0) ||
            !(threads[i].joiner_tid == -1) ||
            !(threads[i].qnext == 0)
        ) {
            printf(1, "[FAIL] unexpected values in thread at idx %d\n", 1);
            exit();
        }
    }
}

int main(void) {
    printf(1, "=== thread suite ====\n");

    run_ok("thread_init ok", thread_init_ok);

    printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
    exit();
}