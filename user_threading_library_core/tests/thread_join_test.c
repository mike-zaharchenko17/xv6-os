#include "types.h"
#include "user.h"
#include "uthreads.h"

static void *worker_slow(void *arg) {
    // yield a couple times so main has a chance to try joining while we're alive
    thread_yield();
    thread_yield();
    return (void*)0x11111111;
}

static void *worker_fast(void *arg) {
    return (void*)0x22222222;
}

int main(void) {
    thread_init();

    int tid1 = thread_create(worker_slow, 0);
    printf(1, "created tid1=%d (slow)\n", tid1);

    // try joining immediately; should block until worker_slow returns.
    void *r1 = thread_join(tid1);
    if (r1 != (void*)0x11111111) {
        printf(1, "join slow: BAD retval %p\n", r1);
        exit();
    }
    printf(1, "join slow: OK retval %p\n", r1);

    int tid2 = thread_create(worker_fast, 0);
    printf(1, "created tid2=%d (fast)\n", tid2);

    // let worker_fast run and exit first
    thread_yield();

    // now join should return immediately (already zombie)
    void *r2 = thread_join(tid2);
    if (r2 != (void*)0x22222222) {
        printf(1, "join fast: BAD retval %p\n", r2);
        exit();
    }
    printf(1, "join fast: OK retval %p\n", r2);

    printf(1, "thread_join_test: PASS\n");
    exit();
}
