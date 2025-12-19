#include "uthreads.h"
#include "stat.h"
#include "types.h"
#include "user.h"

struct thread threads[MAX_THREADS];
struct thread *current_thread = 0;
int next_tid = 1;

struct mutex_t *mutex = 0;
struct sem_t *semaphore = 0;

void thread_switch(struct thread *old, struct thread *next);

static struct thread *find_by_tid(int tid) {
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

  // Set up the main thread (caller of thread_init) as thread 0
  current_thread = &threads[0];
  current_thread->tid = 0;
  current_thread->tstate = T_RUNNING;
}

int thread_self(void) { return current_thread->tid; }

void thread_schedule(void) {
  struct thread *old = current_thread;
  struct thread *next = 0;

  // idx of the current thread in the threads array
  int oldi = (int)(old - threads);

  // Round-Robin Scheduler
  // Iterate through threads starting from the one after the current thread.
  // We use modulo arithmetic to wrap around the array.
  for (int k = 1; k <= MAX_THREADS; k++) {
    // take the offset into the array and add our idx
    // element to it; mod it by max_threads to ensure
    // that we wrap around and never go out of bounds (i.e.,
    // we'll always come back to 0 and then go forward from
    // 0 if we exceed MAX_THREADS)

    // Calculate the index 'i' to check.
    // (oldi + k) moves the index forward by k steps.
    // % MAX_THREADS ensures we wrap around to the beginning if we exceed the
    // array bounds.
    int i = (oldi + k) % MAX_THREADS;

    // If we find a runnable thread, select it and stop searching
    if (threads[i].tstate == T_RUNNABLE) {
      next = &threads[i];
      break;
    }
  }

  // if we exit loop, no one can run, so just
  // either continue current or exit if current is not running
  if (next == 0) {
    if (old->tstate == T_RUNNING) {
      return; // No other thread to run, continue with current
    }
    exit(); // Current thread is blocked/zombie and no one else to run -> Exit
            // process
  }

  // unschedule old thread
  if (old->tstate == T_RUNNING) {
    old->tstate = T_RUNNABLE; // Mark old thread as ready to run later
  }

  // if old was SLEEPING or ZOMBIE, leave it alone.

  // set new thread to running
  next->tstate = T_RUNNING;
  current_thread = next;

  // switch context (when this returns, we're back on some other schedule return
  // path) This calls the assembly function to swap stacks/registers
  thread_switch(old, next);
}

void thread_yield(void) {
  // Voluntarily give up CPU. Mark self as RUNNABLE and call scheduler.
  current_thread->tstate = T_RUNNABLE;
  thread_schedule();
}

void thread_exit(void *retval) {
  current_thread->retval = retval;
  current_thread->tstate = T_ZOMBIE; // Mark as Zombie, wait for join

  // If another thread is waiting to join this one, wake it up
  if (current_thread->joiner_tid != -1) {
    struct thread *joiner = find_by_tid(current_thread->joiner_tid);
    // prevent waking a freed or reused joiner
    if (joiner) {
      joiner->tstate = T_RUNNABLE;
    }
  }

  // Schedule next thread. This function will NOT return since this thread is
  // now ZOMBIE.
  thread_schedule();

  exit(); // Should not be reached
}

static void thread_trampoline(void) {
  // call the current thread's start routine with its saved arg
  void *ret = current_thread->start_routine(current_thread->arg);
  // when start_routine returns, call exit to save this return value
  thread_exit(ret);
}

int thread_create(void *(*start_routine)(void *), void *arg) {
  int idx = -1;
  // find a free slot; skip 0 (main thread)
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

  // point to the top of the stack (stacks grow down)
  uint *sp = (uint *)(stk + STACK_SIZE);

  // we want to spoof a stack for a thread that does not yet have a stack
  // so that thread_switch can treat it like a thread that was previously
  // running

  // we can set thread_trampoline as that return address
  // so that it "returns" into thread_trampoline and start running
  // C code
  *--sp = (uint)thread_trampoline;

  // Push fake values for callee-saved registers (ebp, ebx, esi, edi)
  // These will be popped by thread_switch when switching TO this thread.
  *--sp = 0; // ebp
  *--sp = 0; // ebx
  *--sp = 0; // esi
  *--sp = 0; // edi

  t->sp = (uint)sp; // Save the stack pointer

  return t->tid;
}

void *thread_join(int tid) {
  if (tid == current_thread->tid) {
    printf(1, "invalid target tid; cannot self-join");
    return 0;
  }

  struct thread *target_thread = find_by_tid(tid);

  // find the thread with the target TID

  if (target_thread == 0) {
    printf(1, "invalid target tid; thread not found");
    return 0;
  }

  if (target_thread->tstate == T_UNUSED) {
    printf(1, "invalid target tid; thread is unused");
    return 0;
  }

  if (target_thread->joiner_tid != -1 &&
      target_thread->joiner_tid != current_thread->tid) {
    printf(1, "invalid target tid; thread already has a joiner");
    return 0;
  }

  target_thread->joiner_tid = current_thread->tid;

  // sleep until the target thread exits
  while (target_thread->tstate != T_ZOMBIE) {
    current_thread->tstate = T_SLEEPING;
    thread_schedule(); // Yield CPU until awakened
  }

  void *ret = target_thread->retval;

  // Clean up the thread
  reset_slot(target_thread);

  // return retval to caller; retval is a void pointer so the compiler won't
  // complain
  return ret;
}

/* Wait queue helpers (generalized for all structures) */
static void wait_q_enqueue(wait_q_t *q, struct thread *t) {
  t->qnext = 0;

  if (q->qtail) {
    q->qtail->qnext = t; // Append to tail
    q->qtail = t;
  } else {
    q->qhead = q->qtail = t; // First element
  }
}

static struct thread *wait_q_dequeue(wait_q_t *q) {
  struct thread *t = q->qhead;

  if (!t) {
    return 0;
  }

  q->qhead = t->qnext; // Advance head

  if (q->qhead == 0) {
    q->qtail = 0; // Queue empty
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

  // Logic: Check if anyone is waiting
  if (!waiter) {
    // CASE 1: No one is waiting.
    // Simply unlock.
    m->locked = 0;
    m->owner = 0;
    return;
  }

  // handoff
  // CASE 2: Someone IS waiting.
  // Transfer ownership directly to the waiter.
  // m->locked remains 1 (still locked, but now owned by waiter).
  m->locked = 1;
  m->owner = waiter;
  waiter->tstate = T_RUNNABLE; // Wake up waiter
}

/* SEMAPHORE IMPLEMENTATION */

void sem_init(sem_t *s, int value) {
  if (value < 0) {
    printf(1, "sem_init: value must be >= 0\n");
    exit();
  }
  s->count = value;
  s->q.qhead = 0;
  s->q.qtail = 0;
}

void sem_wait(sem_t *s) {
  s->count--;

  if (s->count < 0) {
    // Count negative means we must wait
    wait_q_enqueue(&s->q, current_thread);
    current_thread->tstate = T_SLEEPING;
    thread_schedule();
  }
}

void sem_post(sem_t *s) {
  if (s->count++ < 0) {
    // Count was negative, so someone is waiting. Wake them up.
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
  if (m->owner != current_thread || m->locked != 1) {
    printf(1, "cond_wait: must be called while mutex is locked\n");
    exit();
  }

  wait_q_enqueue(&c->q, current_thread);

  // it can't be preempted since it's a cooperative model, so this section
  // should be 'atomic' even if there are no explicit guards in place to make it
  // so.

  // thread_yield and/or thread_schedule are being not called in auxillary
  // functions such as mutex_unlock

  // Release mutex before sleeping
  mutex_unlock(m);

  current_thread->tstate = T_SLEEPING;

  thread_schedule();

  /*
  standard behavior:
  the caller should loop on the predicate i.e.,

  mutex_lock(m)
  while (!ready) {
      cond_wait(c, m)
  }
  ... now proceed
  mutex_unlock(m);

  because cond_t is not aware of what the condition actually is- it is just
  a 'place' for threads to sleep.

  the actual condition is a shared predicate that the mutex protects, so
  we need to check that when the thread is signalled and wakes back up

  a wakeup does not guarantee that the condition is met
  */

  // Re-acquire mutex after waking
  mutex_lock(m);

  return;
}

void cond_signal(cond_t *c) {
  struct thread *waiter = wait_q_dequeue(&c->q);
  if (waiter)
    waiter->tstate = T_RUNNABLE;
}

void cond_broadcast(cond_t *c) {
  while (c->q.qhead != 0) {
    struct thread *waiter = wait_q_dequeue(&c->q);
    if (!waiter)
      break;
    waiter->tstate = T_RUNNABLE;
  }
}

/* CHANNEL IMPLEMENTATION */

channel_t *channel_create(int capacity) {
  if (capacity <= 0) {
    return 0;
  }

  // Allocate the channel object first
  channel_t *ch = (channel_t *)malloc(sizeof(channel_t));
  if (!ch) {
    return 0;
  }

  // Allocate backing store for the ring buffer
  ch->buf = (void **)malloc(sizeof(void *) * capacity);
  if (!ch->buf) {
    free(ch);
    return 0;
  }

  // Initialize ring buffer metadata
  ch->capacity = capacity;
  ch->count = 0;
  ch->head = 0;
  ch->tail = 0;
  ch->closed = 0;

  // Initialize synchronization primitives guarding the channel
  mutex_init(&ch->lock);
  cond_init(&ch->not_empty);
  cond_init(&ch->not_full);

  return ch;
}

int channel_send(channel_t *ch, void *data) {
  if (!ch) {
    return -1;
  }

  mutex_lock(&ch->lock);

  // Back-pressure: wait while the buffer is at capacity
  while (!ch->closed && ch->count == ch->capacity) {
    // cond_wait atomically releases the lock and sleeps
    cond_wait(&ch->not_full, &ch->lock);
  }

  // After waking, fail fast if a close was observed
  if (ch->closed) {
    mutex_unlock(&ch->lock);
    return -1;
  }

  // Insert data at tail
  ch->buf[ch->tail] = data;
  // Circular buffer logic: wrap tail index
  ch->tail = (ch->tail + 1) % ch->capacity;
  ch->count++;

  // Signal data is available
  cond_signal(&ch->not_empty);
  mutex_unlock(&ch->lock);

  return 0;
}

int channel_recv(channel_t *ch, void **data) {
  if (!ch || !data) {
    return -1;
  }

  mutex_lock(&ch->lock);

  // Block while empty; a close will break us out
  while (ch->count == 0 && !ch->closed) {
    // Channel empty, wait on not_empty condition
    cond_wait(&ch->not_empty, &ch->lock);
  }

  // If still empty but closed, report termination to caller
  if (ch->count == 0 && ch->closed) {
    mutex_unlock(&ch->lock);
    return -1;
  }

  // Read data from head
  *data = ch->buf[ch->head];
  // Circular buffer logic: wrap head index
  ch->head = (ch->head + 1) % ch->capacity;
  ch->count--;

  // Signal space is available
  cond_signal(&ch->not_full);
  mutex_unlock(&ch->lock);

  return 0;
}

void channel_close(channel_t *ch) {
  if (!ch) {
    return;
  }

  mutex_lock(&ch->lock);
  ch->closed = 1;

  // Wake up everyone
  cond_broadcast(&ch->not_full);
  cond_broadcast(&ch->not_empty);

  mutex_unlock(&ch->lock);
}
