#include "types.h"
#include "user.h"
#include "uthreads.h"

static mutex_t m;
static volatile int counter = 0;

#define NTHREADS 4
#define NITERS   1000

// without the mutex, the tmp = counter; yield; counter = tmp+1; 
// will lose increments constantly.

static void *worker(void *arg) {
    int id = (int)arg;

    for (int i = 0; i < NITERS; i++) {
        mutex_lock(&m);

        // begin critical section
        int tmp = counter;
        thread_yield();          // force a context switch while holding lock
        counter = tmp + 1;
        // end critical section

        mutex_unlock(&m);

        // optional: encourage mixing
        if ((i % 50) == 0)
            thread_yield();
    }

    printf(1, "worker %d done\n", id);
    return 0;
}

int main(void) {
    thread_init();
    mutex_init(&m);

    int tids[NTHREADS];
    for (int i = 0; i < NTHREADS; i++) {
        tids[i] = thread_create(worker, (void*)i);
        printf(1, "created worker tid=%d\n", tids[i]);
    }

    for (int i = 0; i < NTHREADS; i++) {
        thread_join(tids[i]);
    }

    int expected = NTHREADS * NITERS;
    printf(1, "counter=%d expected=%d\n", counter, expected);

    if (counter != expected) {
        printf(1, "MUTEX TEST FAIL\n");
        exit();
    }

    printf(1, "MUTEX TEST PASS\n");
    exit();
}