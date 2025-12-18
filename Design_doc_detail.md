# xv6 User-Level Threading Library: Detailed Design & Implementation

**Authors:** Athul R, Mike Zaharchenko
**Course:** CS-GY 6233 Operating Systems (Fall 2025)

---

## 1. Introduction

This document provides an in-depth look at the implementation of our user-level threading library for xv6. Unlike the high-level summary, this document dives into the specific logic, code-level decisions, and verification strategies we employed. We built this library to support **N:1 threading**, meaning multiple user threads share a single kernel process.

## 2. Part 1: The Threading Core

The foundation of our library is the ability to multiplex multiple execution contexts onto a single CPU.

### 2.1 The Thread Control Block (`struct thread`)

We designed the `struct thread` to be self-contained. It holds not just the state, but the memory (stack) required for execution.

```c
struct thread {
  uint sp;                   // [CONTEXT] Saved Stack Pointer
  char *stack;               // [MEMORY] Pointer to the allocated stack (8KB)
  enum threadstate tstate;   // [LIFECYCLE] Current state (RUNNING, RUNNABLE, etc.)
  int tid;                   // [ID] Thread ID
  
  // Execution metadata
  void *(*start_routine)(void *); // Function to run
  void *arg;                      // Argument to pass
  void *retval;                   // Exit value
  
  // Join mechanism
  int joiner_tid;            // TID of the thread waiting for us to die
  
  // Queue linkage
  struct thread *qnext;      // For wait queues (Mutex/Cond/Sem)
};
```

**Why `sp` is crucial:** In user-level threading, we don't trap into the kernel to switch threads. We simply swap the stack pointer (`%esp`) register. By saving `sp`, we save the entire call stack and local variables of the thread.

### 2.2 Stack Spoofing (Thread Creation)

The most "magical" part of `thread_create` is setting up the stack for a new thread. Since this thread has never run before, it has no saved context. We must **fake** a context so that `thread_switch` can "return" into it later.

**Our Implementation Logic (`thread_create` in `uthreads.c`):**

```c
  // 1. Allocate stack
  char *stk = (char *)malloc(STACK_SIZE);
  uint *sp = (uint *)(stk + STACK_SIZE); // Point to top (stacks grow down)

  // 2. Push return address: thread_trampoline
  // When thread_switch performs 'ret', the CPU will pop this address and jump there.
  *--sp = (uint)thread_trampoline;

  // 3. Push fake registers (EDI, ESI, EBX, EBP)
  // thread_switch expects to pop these 4 values before returning.
  *--sp = 0; // ebp
  *--sp = 0; // ebx
  *--sp = 0; // esi
  *--sp = 0; // edi

  // 4. Save this pointer
  t->sp = (uint)sp;
```

**The Trampoline:** We pointed the return address to `thread_trampoline`. This wrapper function actually calls the user's function. This ensures that when the user function `return`s, we catch it and call `thread_exit`.

```c
static void thread_trampoline(void) {
  void *ret = current_thread->start_routine(current_thread->arg);
  thread_exit(ret); // Ensure we clean up even if user forgot to call exit
}
```

### 2.3 Context Switching (`thread_switch.S`)

We implemented the context switch in assembly because C cannot directly manipulate the `%esp` register without messing up its own stack frame.

**The Code:**
```assembly
thread_switch:
    # 1. Save current registers (Callee-saved only)
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # 2. Get arguments: void thread_switch(struct thread *old, struct thread *next);
    movl 20(%esp), %eax # eax = old
    movl 24(%esp), %edx # edx = next

    # 3. Save current SP into old->sp
    movl %esp, 0(%eax)

    # 4. LOAD next SP from next->sp (CRITICAL MOMENT)
    movl 0(%edx), %esp  # <--- Context Switch happens here

    # 5. Restore registers from the NEW stack
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    # 6. Return into the new thread
    ret
```

### 2.4 Scheduling (Round-Robin)

Our scheduler is simple and fair. It loops through the `threads` array starting from the *current* position + 1.

**Logic:**
```c
  // iterate from k=1 to MAX_THREADS
  int i = (oldi + k) % MAX_THREADS; 
  if (threads[i].tstate == T_RUNNABLE) {
      next = &threads[i];
      break;
  }
```
This ensures that if we have threads [A, B, C] and A yields, we check B, then C, then A. No thread is starved.

### 2.5 Unit Tests (`thread_unit_tests.c`)

We verified Part 1 with the following tests:
1.  **`thread_init_ok`**: Checks that the main process is correctly wrapped as Thread 0.
2.  **`thread_create_ok`**: Creates 3 threads. Since they all call `thread_yield()`, we manually verify via `printf` that their execution interleaves (e.g., A runs, yields, B runs, yields...).
3.  **`thread_join_ok`**:
    *   *Scenario 1:* Join a running thread. We verified that the caller BLOCKS until the target finishes.
    *   *Scenario 2:* Join a dead (zombie) thread. We verified it returns immediately with the correct value.

## 3. Part 2: Synchronization Primitives

### 3.1 Mutexes: The Handoff Optimization

We implemented mutexes (`mutex_t`) to protect critical sections. A naive implementation wakes up a thread and lets it retry `lock()`. This is inefficient (spinning/barging).

**Our Approach (Handoff):**
When `mutex_unlock` is called, we don't set `locked = 0` if there is a waiter. Instead, we keep `locked = 1` and simply **change the owner** to the waking thread.

**Code Illustration (`uthreads.c`):**
```c
void mutex_unlock(mutex_t *m) {
  struct thread *waiter = wait_q_dequeue(&m->q);
  
  if (waiter) {
    // HANDOFF: Transfer ownership directly
    m->owner = waiter; 
    m->locked = 1;       // Remains locked!
    waiter->tstate = T_RUNNABLE; 
  } else {
    // No one waiting, simply unlock
    m->locked = 0;
    m->owner = 0;
  }
}
```
This guarantees the Awakened thread gets the lock next, preventing starvation.

**Verification (`mutex_unit_tests.c`):**
*   **`mutex_protects_counter_ok`**: We spawned 4 threads, each incrementing a shared `counter` 1000 times.
    *   *Result:* Final count was exactly 4000.
    *   *Control:* `unprotected_counter_test_fails` runs without mutexes and consistently yields < 4000 due to race conditions.

### 3.2 Semaphores

Our semaphores support counting resources.
*   **Negative Count logic:** We intentionally let the internal `count` go negative. `count = -3` means "3 threads are waiting". This makes logic cleaner.

**Verification (`semaphore_unit_tests.c`):**
*   **`semaphore_fifo_test_ok`**: We proved that our wait queue is FIFO. We had Threads 1, 2, 3 call `sem_wait`. We called `sem_post` 3 times and verified they woke up in order 1 -> 2 -> 3.

### 3.3 Condition Variables

The trickiest part of Condition Variables (CV) is the atomic release-and-sleep.

**Our Implementation:**
```c
void cond_wait(cond_t *c, mutex_t *m) {
  wait_q_enqueue(&c->q, current_thread);
  mutex_unlock(m);          // 1. Release lock
  current_thread->tstate = T_SLEEPING; 
  thread_schedule();        // 2. Sleep
  mutex_lock(m);            // 3. Re-acquire upon waking
}
```
*   **Why is this safe?** In a kernel implementation, there's a race between 1 and 2. But since we are cooperative (user-level), no one can interrupt us between `mutex_unlock` and `thread_schedule` unless we yield. We don't yield. So it's effectively atomic.

### 3.4 Channels (The "Go" Style)

We implemented `channel_t` as a ring buffer (circular queue) protected by a mutex and two CVs (`not_empty`, `not_full`).

**Logic (`channel_send`):**
```c
mutex_lock(&ch->lock);
while (ch->count == ch->capacity) {
    cond_wait(&ch->not_full, &ch->lock); // Block if full
}
ch->buf[ch->tail] = data;
ch->tail = (ch->tail + 1) % ch->capacity; // Wrap around
ch->count++;
cond_signal(&ch->not_empty); // Wake receiver
mutex_unlock(&ch->lock);
```

**Verification (`channel_tests.c`):**
*   **`test_block_on_full_then_recv`**: Created a channel of size 1.
    *   Filled it.
    *   Tried to send again -> Verified sender BLOCKS.
    *   Received one item -> Verified sender WAKES and completes.

## 4. Part 3: Advanced Concurency Problems

### 4.1 Reader-Writer Lock (Writer Priority)

The standard problem allows readers to starve writers. We implemented **Writer Priority**.

**The Logic:**
A reader is allowed to enter IF AND ONLY IF:
1.  No writer is writing.
2.  No writer is **waiting**.

```c
// reader_lock() from rw_lock.c
mutex_lock(&s->lock);
// The 's->writers_waiting > 0' check enforces priority
while (s->writer_active || s->writers_waiting > 0) {
    cond_wait(&s->readers_ok, &s->lock);
}
s->readers_active++;
mutex_unlock(&s->lock);
```

**Example Output (`rw_lock.c`):**
```
Reader 1: reading value = 0   (Runs)
Writer 1: arriving...         (Waits, but increments writers_waiting)
Reader 2: arriving...         (Sees writers_waiting > 0, BLOCKs even though lock is free!)
Writer 1: wrote new value = 1 (Runs)
Reader 2: reading value = 1   (Runs after writer)
```

## 5. Part 4: Thread-Safe File I/O (Async Simulation)

Implementing `read` that doesn't block the whole process is impossible in pure user-space threads without non-blocking syscalls (which we couldn't easily add).

**Our Solution: Multi-Process Producer-Consumer**
We split the problem into two xv6 processes connected by a pipe.

1.  **Process 1 (Producer):** Runs multiple threads. They lock `write_lock` and write to the pipe.
2.  **Process 2 (Consumer):** Runs multiple threads. They lock `read_lock` and read from the pipe.

**Why this works:** if Consumer Process blocks on `read()` (empty pipe), the OS schedules the Producer Process. The Producer generates data. This achieves the goal: one thread's blocking I/O does not halt the entire application's progress (because the application is split across processes).

**Code (`thread_safe_file_io.c`):**
```c
if (pid == 0) {
    producer_process(); // Spawns N threads, writes to pipe
} else {
    consumer_process(); // Spawns N threads, reads from pipe
}
```

---

## 6. Conclusion

We successfully implemented a fully functional threading library in xv6. The library is robust, strictly following the N:1 model with cooperative scheduling. We demonstrated its correctness through rigorous unit testing (covering mutex counting, semaphore FIFO ordering, and channel blocking) and solved complex synchronization problems like Writer-Priority RW locks. The code is modular, separating the interface (`uthreads.h`) from implementation (`uthreads.c`), ensuring clean abstraction.
