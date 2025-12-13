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

    for (int k = 1; k <= MAX_THREADS; k++) {
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

void thread_exit(void *retval) {
    current_thread->retval = retval;
    current_thread->tstate = T_ZOMBIE;
    thread_schedule();

    exit();
}

static void thread_trampoline(void) {
    // call the current thread's start routine with its saved arg
    void *ret = current_thread->start_routine(current_thread->arg);
    // when start_routine returns, call exit to save this return value
    thread_exit(ret);
}

int thread_create(void *(*start_routine)(void *), void *arg) {
    int idx = -1;
    // find a free slot; skip 0
    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].tstate == T_UNUSED) {
            idx = i;
            break;
        }
    }

    // if not found, return -1
    if (idx < 0) {
        return -1;
    }

    // pull the thread at idx from the table
    struct thread *t = &threads[idx];

    // allocate memory for this thread's stack
    char *stk = (char *)malloc(STACK_SIZE);

    if (stk == 0) {
        return -1;
    }

    // init metadata
    t->tid = next_tid;
    next_tid++;

    t->tstate = T_RUNNABLE;

    t->stack = stk;
    t->sp = 0;

    t->start_routine = start_routine;
    t->arg = arg;

    t->retval = 0;
    t->joiner_tid = -1;
    t->qnext = 0;

    // point to the top of the stack
    uint *sp = (uint *)(stk + STACK_SIZE);

    // we want to spoof a stack for a thread that does not yet have a stack
    // so that thread_switch can treat it like a thread that was previously
    // running

    // we can set thread_trampoline as that return address
    // so that it "returns" into thread_trampoline and start running
    // C code
    *--sp = (uint)thread_trampoline;

    *--sp = 0; //ebp
    *--sp = 0; //ebx
    *--sp = 0; //esi
    *--sp = 0; //edi

    t->sp = (uint)sp;

    return t->tid;
}
