#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].tstate = T_UNUSED;
        threads[i].stack = 0;
        threads[i].sp = 0;
    }

    current_thread = &threads[0];
    current_thread->tid = 0;
    current_thread->tstate = T_RUNNING;
    current_thread->stack = 0;
    current_thread->sp = 0;
}

int thread_self(void) {
    return current_thread->tid;
}