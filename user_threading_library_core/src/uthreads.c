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
    printf(1, "[uthreads.c/thread_schedule] oldi = %d\n", oldi);

    for (int k = 1; k <= MAX_THREADS; k++) {
        // take the offset into the array and add our idx
        // element to it; mod it by max_threads to ensure
        // that we wrap around and never go out of bounds (i.e.,
        // we'll always come back to 0 and then go forward from 
        // 0 if we exceed MAX_THREADS)

        int i = (oldi + k) % MAX_THREADS;
        printf(1, "[uthreads.c]/thread_schedule] loop variable i is at %d\n", i);
        if (threads[i].tstate == T_RUNNABLE) {
            printf(1, "[uthreads.c/thread_schedule] found runnable thread at %d\n", i);
            next = &threads[i];
            break;
        }
    }

    printf(1, "[uthreads.c/thread_schedule] exited loop\n");

    // if we exit loop, no one can run, so just
    // either continue current or exit if current is not running
    if (next == 0) {
        printf(1, "[uthreads.c/thread_schedule] no runnable thread found\n");
        if (old->tstate == T_RUNNING) {
            printf(1, "[uthreads.c/thread_schedule] old thread is running; returning\n");
            return;
        }
        printf(1, "[uthreads.c/thread_schedule] nothing to schedule; exiting\n");
        exit();
    }

    // unschedule old thread
    if (old->tstate == T_RUNNING) {
        printf(1, "[uthreads.c/thread_schedule] unscheduling old thread; setting state to runnable\n");
        old->tstate = T_RUNNABLE;
    }

    // if old was SLEEPING or ZOMBIE, leave it alone.

    // set new thread to running
    next->tstate = T_RUNNING;
    current_thread = next;

    // switch context (when this returns, we're back on some other schedule return path)
    printf(1, "[uthreads.c/thread_schedule] switching to new thread\n");
    thread_switch(old, next);
}

void thread_yield(void) {
    printf(1, "[uthreads.c/thread_yield] yielding from thread %d\n", current_thread->tid);
    current_thread->tstate = T_RUNNABLE;
    printf(1, "[uthreads.c/thread_yield] calling scheduler\n");
    thread_schedule();
}