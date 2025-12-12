#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].tid = -1;
        threads[i].tstate = T_UNUSED;

        threads[i].stack = 0;
        threads[i].sp = 0;

        threads[i].start_routine = 0;
        threads[i].arg = 0;
        threads[i].retval = 0;

        threads[i].joiner_tid = -1;
        threads[i].qnext = 0;
    }

    current_thread = &threads[0];
    current_thread->tid = 0;
    current_thread->tstate = T_RUNNING;
}

int thread_self(void) {
    return current_thread->tid;
}