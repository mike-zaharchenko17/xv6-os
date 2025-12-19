# xv6 User-Level Threading Library: Detailed Design & Implementation

**Authors:** Athul R, Mike Zaharchenko
**Course:** CS-GY 6233 Operating Systems (Fall 2025)

---
## 0. Prelogue
### Build (compiles everything)
1. From the repo root, build the filesystem image (this also compiles all user tests):
   ```sh
   make fs.img
   ```
2. Launch xv6 in a terminal:
   ```sh
   make qemu-nox
   ```
   (Use `make qemu` if you prefer the graphical console.)



### Notes
- All user tests are statically linked and copied into `fs.img` by `make fs.img`; no extra compile steps are needed beyond `make fs.img`.
- If you add or modify tests, rerun `make fs.img` before booting xv6 to ensure the new binaries are on the disk image.
- Use `ls` inside xv6 to see the truncated names if you forget them. ***


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

The most "critical" part of `thread_create` is setting up the stack for a new thread. Since this thread has never run before, it has no saved context. We must **fake** a context so that `thread_switch` can "return" into it later.

**Our Implementation Logic (`thread_create` in `uthreads.c`):**

```c
  // 1. Allocate stack
  char *stk = (char *)malloc(STACK_SIZE);
  uint *sp = (uint *)(stk + STACK_SIZE); // Point to top (stacks grow down)

  // 2. Push return address: thread_trampoline
  // When thread_switch performs 'ret', the CPU will pop this address and jump there.
  *--sp = (uint)thread_trampoline;

  // 3. Push fake registers (EBP, EBX, ESI, EDI) in the exact pop order
  // used by thread_switch; this keeps the spoofed frame consistent with the
  // assembly routine.
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

We implemented `channel_t` to facilitate safe inter-thread communication using a bounded buffer model. This approach decouples data production from consumption and handles synchronization internally, reducing the complexity for the end-user.

#### Core Design Architecture
The channel is implemented as a **fixed-size circular buffer**. This structure was chosen to provide O(1) enqueue and dequeue operations while maintaining a strictly bounded memory footprint.

To ensure thread safety and correct blocking behavior, we employed the following synchronization primitives:
1.  **Mutex (`lock`)**: Guarantees mutual exclusion for all internal state modifications (buffer access, index updates).
2.  **`not_full` Condition Variable**: Queues sender threads when the buffer reaches capacity.
3.  **`not_empty` Condition Variable**: Queues receiver threads when the buffer is empty.

#### Sending Logic (`channel_send`)

The `channel_send` operation is designed to block the caller if the channel cannot accept new data.

1.  **State Protection**: We acquire the mutex immediately to ensure atomic inspection of the `count` and `capacity`.
2.  **Flow Control (Blocking)**:
    We check `count == capacity`. If true, the thread must wait. We use `cond_wait` on the `not_full` condition variable, which atomically releases the lock and suspends the thread. This atomicity is critical; it prevents a "missed wakeup" race condition where a receiver might signal `not_full` before the sender actually goes to sleep.
    Upon waking, we re-evaluate the full condition in a `while` loop to handle spurious wakeups or intervening senders.
3.  **Data Atomic Commit**:
    Once space is guaranteed, we write the data to the buffer at the `tail` index and update the state (`tail` increments modulo capacity, `count` increments) within the same critical section.
4.  **Wakeup Signaling**:
    After a successful write, we signal `not_empty`. We confirm `count > 0`, implying a receiver might be blocked waiting for data.
5.  **Lock Release**: The mutex is released only after all state updates and signals are complete.

#### Receiving Logic (`channel_recv`)

The receive logic mirrors the send logic to ensure symmetry and correctness.

1.  **State Protection**: The mutex is acquired to read the channel state safely.
2.  **Flow Control (Blocking)**:
    We check `count == 0`. If the buffer is empty, the thread waits on the `not_empty` condition variable. This releases the resource (the lock) to allow senders to populate the buffer.
3.  **Data Retrieval**:
    We read from the `head` index, perform the modular increment of `head`, and decrement `count`.
4.  **Wakeup Signaling**:
    Since we have consumed an item, we signal `not_full` to wake any potentially blocked senders.
5.  **Lock Release**: The mutex is released, returning control to the scheduler.

**Verification (`channel_tests.c`):**
*   **`test_block_on_full_then_recv`**: Created a channel of size 1.
    *   Filled it.
    *   Tried to send again -> Verified sender BLOCKS.
    *   Received one item -> Verified sender WAKES and completes.

## 4. Part 3: Advanced Concurency Problems

### 4.1 Reader-Writer Lock (Writer Priority)

The standard Reader-Writer lock allows concurrent read access but exclusive write access. A naive implementation often prioritizes readers (allowing them to enter as long as no writer *holds* the lock). We identified that this leads to **Writer Starvation** under high contention.

To resolve this, we implemented **Writer Priority** logic.

#### Writer Priority Design
Our design enforces a strict policy: **New readers are blocked if a writer is either active OR waiting.** This ensures that once a writer declares intent to acquire the lock, it will be the next (or near-next) to run, regardless of incoming readers.

We extended the synchronization state to include `writers_waiting`.

#### Implementation Logic

**1. The Writer's Path (`writer_lock`)**
The purpose of the writer lock is to gain exclusive access efficiently.
*   **Registration**: We immediately increment `s->writers_waiting` upon entry. This is the critical step for priority; it signals to all incoming readers that a writer is pending.
*   **Wait Condition**: The writer waits strictly until `writer_active` is false AND `readers_active` is 0.
*   **Acquisition**: Once the condition is met, we decrement `writers_waiting` and set `writer_active = 1`.

**2. The Reader's Path (`reader_lock`)**
The reader lock is designed to yield to writers.
*   **Admission Test**:
    ```c
    while (s->writer_active || s->writers_waiting > 0) {
        cond_wait(&s->readers_ok, &s->lock);
    }
    ```
    We check not only if a writer is *writing* (`writer_active`) but also if one is *queued* (`writers_waiting > 0`). If either is true, the reader waits. This logic prevents the "stream of readers" problem that causes writer starvation.

**3. Ownership Handoff (`writer_unlock`)**
Efficiency in waking threads is determined by our priority policy.
*   When a writer releases the lock, we check `writers_waiting`.
*   **Priority Wakeup**: If `writers_waiting > 0`, we signal `writers_ok` to wake exactly one writer.
*   **Secondary Wakeup**: Only if no writers are waiting do we use `cond_broadcast(&s->readers_ok)` to wake all readers. This effectively batches reader execution only during periods of no write contention.

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

## Appendix: Test Binaries & Names (current layout)
- **Unit tests (under `user_threading_library_core/tests/`)**: `t_thread_unit_`, `t_mutex_unit_t`, `t_semaphore_un`, `t_channel_test`, `t_cond_var_uni` (14-char truncation applied by xv6).
- **Examples/demos (under `user_threading_library_core/examples/`)**: `t_pc_sem`, `t_pc_chan`, `t_producer_con` (producer_consumer_problem), `t_rw_lock`, `t_thread_safe_` (thread_safe_file_io).
- UPROGS in `Makefile` now point directly at these sources so `make fs.img` copies the up-to-date binaries onto the disk image.

## 6. Conclusion

We successfully implemented a fully functional threading library in xv6. The library is robust, strictly following the N:1 model with cooperative scheduling. We demonstrated its correctness through rigorous unit testing (covering mutex counting, semaphore FIFO ordering, and channel blocking) and solved complex synchronization problems like Writer-Priority RW locks. The code is modular, separating the interface (`uthreads.h`) from implementation (`uthreads.c`), ensuring clean abstraction.


### Running tests inside xv6
At the xv6 shell prompt (`$`), run each test by its truncated name (shown in the table). Example:
```
$ t_channel_test
```
Exit xv6 with `Ctrl-a x`.

### Test catalog
| Source file (full) | xv6 binary name | Purpose |
| user_threading_library_core/tests/semaphore_unit_tests.c | t_semaphore_unit_tests | Semaphore wait/post behavior. |
| user_threading_library_core/tests/thread_unit_tests.c | t_thread_unit_tests| Core threading lifecycle (create/join/yield/exit). |
| user_threading_library_core/tests/channel_tests.c | t_channel_tests | Channel basics: send/recv ordering, full-buffer blocking, close wakeups. |
| user_threading_library_core/tests/mutex_unit_tests.c | t_mutex_unit_tests | Core mutex functionality
| user_threading_library_core/tests/cond_var_unit_tests.c | t_cond_var_unit_tests | Core cond var tests

### Example catalog
| Source file (full) | xv6 binary name | Purpose |
| user_threading_library_core/examples/pc_chan.c | t_pc_chan | P/C with channels demo |
| user_threading_library_core/examples/pc_sem.c | t_pc_chan | P/C with semaphore demo |
| user_threading_library_core/examples/producer_consumer_problem.c | t_producer_consumer_problem | P/C with semaphore alternate demo |
| user_threading_library_core/examples/rw_lock.c | t_rw_lock | Writer Priority lock demo |
| user_threading_library_core/examples/thread_safe_file_io.c | t_thread_safe_file_io | File I/O with two processes demo |