#include "types.h"
#include "user.h"
#include "uthreads.h"

static volatile int counter = 0;

#define NTHREADS 4
#define NITERS 1000

static void *worker(void *arg) {
    int id = (int) arg;

    for (int i = 0; i < NITERS; i++) {
        int tmp = counter;
        thread_yield();
        counter = tmp + 1;
        thread_yield();

        if ((i % 50) == 0) {
            thread_yield();
        }
    }

    printf(1, "worker %d done \n", id);

    return 0;
}

int main(void) {
    thread_init();
    
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

    if (counter == expected) {
        printf(1, "RACE TEST: unexpectedly matched (try larger NITERS)\n");
    } else {
        printf(1, "RACE TEST: race observed (counter=%d)\n", counter);
    }

    exit();
}