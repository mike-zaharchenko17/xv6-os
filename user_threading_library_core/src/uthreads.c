#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

struct mutex_t *mutex = 0;
struct sem_t *semaphore = 0;

void thread_switch(struct thread *old, struct thread *next);

static struct thread* find_by_tid(int tid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].tid == tid) {
            return &threads[i];
        }
    }
    return 0;
}

static void reset_slot(struct thread *t) {
    if (t->stack) {
        free(t->stack);
        t->stack = 0;
    }

    t->tid = -1;
    t->sp = 0;
    t->start_routine = 0;
    t->arg = 0;
    t->retval = 0;
    t->joiner_tid = -1;
    t->qnext = 0;
    t->tstate = T_UNUSED;
}

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

    if (current_thread->joiner_tid != -1) {
        struct thread *joiner = find_by_tid(current_thread->joiner_tid);
        // prevent waking a freed or reused joiner
        if (joiner) {
            joiner->tstate = T_RUNNABLE;
        }
    }

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

void *thread_join(int tid) {
    if (tid == current_thread->tid) {
        printf(1, "invalid target tid; cannot self-join");
        return 0;
    }

    struct thread* target_thread = find_by_tid(tid);

    // find the thread with the target TID
    
    if (target_thread == 0) {
        printf(1, "invalid target tid; thread not found");
        return 0;
    }

    if (target_thread->tstate == T_UNUSED) {
        printf(1, "invalid target tid; thread is unused");
        return 0;
    }

    if (target_thread->joiner_tid != -1 && target_thread->joiner_tid != current_thread->tid) {
        printf(1, "invalid target tid; thread already has a joiner");
        return 0;
    }

    target_thread->joiner_tid = current_thread->tid;

    // sleep until the target thread exits
    while (target_thread->tstate != T_ZOMBIE) {
        current_thread->tstate = T_SLEEPING;
        thread_schedule();
    }

    void *ret = target_thread->retval;

    reset_slot(target_thread);

    // return retval to caller; retval is a void pointer so the compiler won't complain
    return ret;
}

/* Wait queue helpers (generalized for all structures) */
static void wait_q_enqueue(wait_q_t *q, struct thread *t) {
    t->qnext = 0;

    if (q->qtail) {
        q->qtail->qnext = t;
        q->qtail = t;
    } else {
        q->qhead = q->qtail = t;
    }
}

static struct thread* wait_q_dequeue(wait_q_t *q) {
    struct thread *t = q->qhead;

    if (!t) {
        return 0;
    }

    q->qhead = t->qnext;

    if (q->qhead == 0) {
        q->qtail = 0;
    }

    t->qnext = 0;
    return t;
}

/* MUTEX IMPLEMENTATION */

void mutex_init(mutex_t *m) {
    m->locked = 0;
    m->q.qhead = 0;
    m->q.qtail = 0;
    m->owner = 0;
}

void mutex_lock(mutex_t *m) {
    // if the current thread already holds mutex, error
    if (m->owner == current_thread) {
        printf(1, "mutex_lock: deadlock (self-lock)\n");
        exit();
    }

    // similar idea to join; if it's locked, enqueue it, put it
    // to sleep, and run the scheduler
    while (m->locked && m->owner != current_thread) {
        wait_q_enqueue(&m->q, current_thread);
        current_thread->tstate = T_SLEEPING;
        thread_schedule();
    }

    // if we got here because of handoff, we already own it.
    if (m->owner == current_thread) {
        return;
    }

    // otherwise, lock the mutex and set the owner to be the current thread 
    // we don't need to modify wait queue because if the mutex is not locked,
    // there is nothing waiting for it

    m->locked = 1;
    m->owner = current_thread;
}

void mutex_unlock(mutex_t *m) {
    if (m->owner != current_thread) {
        printf(1, "mutex_unlock: only the owner can unlock the mutex\n");
        exit();
    }

    struct thread *waiter = wait_q_dequeue(&m->q);

    if (!waiter) {
        m->locked = 0;
        m->owner = 0;
        return;
    }

    // handoff
    m->locked = 1;
    m->owner = waiter;
    waiter->tstate = T_RUNNABLE;
}

/* SEMAPHORE IMPLEMENTATION */

void sem_init(sem_t *s, int value) {
    if (value < 0) {
        printf(1, "sem_init: value must be >= 0");
        exit();
    }
    s->count = value;
    s->q.qhead = 0;
    s->q.qtail = 0;
}

void sem_wait(sem_t *s) {
    s->count--;

    if (s->count < 0) {
        wait_q_enqueue(&s->q, current_thread);
        current_thread->tstate = T_SLEEPING;
        thread_schedule();
    }
}

void sem_post(sem_t *s) {
    if (s->count++ < 0) {
        struct thread *t = wait_q_dequeue(&s->q);
        if (t) {
            t->tstate = T_RUNNABLE;
        }
    }
}

/* CONDITION VARIABLE IMPLEMENTATION */

void cond_init(cond_t *c) {
    c->q.qhead = 0;
    c->q.qtail = 0;
}

void cond_wait(cond_t *c, mutex_t *m) {
    printf(1, "stub for cond_wait");
}

void cond_signal(cond_t *c) {
    struct thread *waiter = wait_q_dequeue(&c->q);
    if (!waiter) {
        printf(1, "cond_signal: queue empty");
        return;
    }
    waiter->tstate = T_RUNNABLE;

}

void cond_broadcast(cond_t *c) {
    printf(1, "stub for cond_broadcast");
}