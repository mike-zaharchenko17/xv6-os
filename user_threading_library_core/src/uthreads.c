#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

void thread_switch(struct thread *old, struct thread *next);

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

void thread_schedule(void) {
    struct thread *old = current_thread;
    struct thread *next = 0;

    // idx of the current thread
    int oldi = (int)(old - threads);
    
    for (int k = oldi; k <= MAX_THREADS; k++) {
        // take the offset into the array and add our idx
        // element to it; mod it by max_threads to ensure
        // that we wrap around and never go out of bounds (i.e.,
        // we'll always come back to 0 and then go forward from 
        // 0 if we exceed MAX_THREADS)

        int i = (oldi + k) % MAX_THREADS;
        if (threads[i].tstate == T_RUNNABLE) {
            next = &threads[i];
            break;
        }
    }

    // if we exit loop, no one can run, so just
    // either continue current or exit if current is not running
    if (next == 0) {
        if (old->tstate == T_RUNNING) {
            return;
        }
        exit();
    }

    // unschedule old thread
    if (old->tstate == T_RUNNING) {
        old->tstate = T_RUNNABLE;
    }

    // if old was SLEEPING or ZOMBIE, leave it alone.

    // set new thread to running
    next->tstate = T_RUNNING;
    current_thread = next;

    // switch context (when this returns, we're back on some other schedule return path)
    thread_switch(old, next);
}

void thread_yield(void) {
    current_thread->tstate = T_RUNNABLE;
    thread_schedule();
}