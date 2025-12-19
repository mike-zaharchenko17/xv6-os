# xv6 User-Level Threading Library Design Document

**Authors:** Athul R, Mike Zaharchenko
**Course:** CS-GY 6233 Operating Systems (Fall 2025)

## 1. Overview

This document details the design and implementation of a user-level threading library for the xv6 operating system. The library provides a robust threading environment entirely in user space, mapping $N$ user threads to $1$ kernel process (Process). It supports thread creation, scheduling, and synchronization primitives (Mutexes, Semaphores, Condition Variables, Channels) necessary for concurrent programming.

The library is designed to clear separation between the **Interface** (what the user calls) and the **Implementation** (how it works internally).

## 2. Architecture

### 2.1 Threading Model (N:1)

We implemented an **N:1 threading model**. The kernel treats the process as a single schedulable entity. Inside the process, our library manages `MAX_THREADS` (32) schedulable units (threads).

*   **Pros:** Fast context switching (no system calls), flexible scheduling.
*   **Cons:** Blocking system calls block the entire process. No true parallelism on multi-core since only one kernel thread (process) runs at a time.

### 2.2 System Architecture

```mermaid
graph TD
    UserApp[User Application] -->|Calls| API[Threading API]
    subgraph "User Space Library"
        API --> Scheduler
        Scheduler -->|Selects| ThreadTable[Thread Table]
        Scheduler -->|Invokes| Switch[Context Switch (ASM)]
        API --> Sync[Sync Primitives]
        Sync -->|Sleeps/Wakes| Scheduler
    end
    subgraph "Kernel Space"
        Process[xv6 Process]
    end
    Switch -.-> Process
```

## 3. Part 1: Threading Foundation

### 3.1 Thread Structure (`struct thread`)

The core of our library is the thread control block (TCB).

```c
struct thread {
  uint sp;                   // Saved Stack Pointer (Context)
  int tid;                   // Thread ID
  enum threadstate tstate;   // current state
  char *stack;               // Pointer to thread's stack memory
  void *(*start_routine)(void *); // Function to run
  void *arg;                 // Function argument
  void *retval;              // Return value for join
  int joiner_tid;            // TID of thread waiting to join this one
  struct thread *qnext;      // For linked-list queues (wait queues)
};
```
**Design Choice - Stack Size:** We chose `8192` bytes (8KB) for `STACK_SIZE`. This is sufficient for typical educational usages while preventing excessive memory consumption per process.

### 3.2 Thread States

We follow a standard state machine:

*   **T_UNUSED:** Slot is free.
*   **T_RUNNABLE:** Ready to run, waiting for scheduler.
*   **T_RUNNING:** Currently executing.
*   **T_SLEEPING:** Blocked on a synchronization primitive.
*   **T_ZOMBIE:** Finished execution, waiting to be joined.

**Comparison:** Standard pthreads do not expose these states, but they are crucial for our internal scheduler.

### 3.3 The Scheduler

We implemented a **Round-Robin** scheduler.
*   **Time Complexity:** $O(N)$ where $N$ is `MAX_THREADS`.
*   **Logic:** It iterates through the thread table starting from `(current_thread_index + 1)`. This ensures fairness. If no other thread is `T_RUNNABLE`, it continues with the current thread or exits if only the current thread is blocked/zombie.

### 3.4 Context Switching (`thread_switch.S`)

Context switching is the only part written in x86 Assembly. Since C functions cannot easily modify their own stack pointer without returning, we use assembly to swap the `%esp`.

**Mechanism:**
1.  **Save Old Context:** Push `ebp`, `ebx`, `esi`, `edi` (Calée-saved registers) onto the *current* stack.
2.  **Save Stack Pointer:** Store current `%esp` into `old->sp`.
3.  **Switch Stack:** Load `%esp` from `next->sp`.
4.  **Restore New Context:** Pop `edi`, `esi`, `ebx`, `ebp` from the *new* stack.
5.  **Return:** The `ret` instruction now pops the return address from the *new* stack, seemingly returning into the function where the new thread last yielded.

**Stack Spoofing:**
When a thread is first created, it has no history. We "spoof" a stack frame so `thread_switch` thinks it's returning from a previous call. We push `thread_trampoline` as the return address and 4 zeros for the registers. `thread_trampoline` handles calling the user's function and then calling `thread_exit`.

## 4. Part 2: Synchronization Primitives

The core philosophy of our primitives is **Cooperative Yielding**. We do not spin; if a lock is busy, we set state to `T_SLEEPING` and yield.

### 4.1 Mutex (`mutex_t`)

*   **Structure:** `locked` flag, `owner` pointer, and `wait_q`.
*   **Locking:** If `locked`, add self to queue, sleep, yield.
*   **Unlocking (Handoff Optimization):** When unlocking, if there are waiters, we **directly transfer ownership** to the next waiter. We **do not** unlock `locked=0` and let them fight for it. This prevents "barging" (where a new thread steals the lock before the woken thread runs) and ensures fairness.

**Comparison with/without Mutex (Shared Counter):**
*   *Without Mutex:* Threads race to read-modify-write the counter. Updates are lost. Final count < Expected.
*   *With Mutex:* Updates are serialized. Final count == Expected.

### 4.2 Semaphores (`sem_t`)

Standard counting semaphore.
*   **Wait:** Decrement count. If negative, block.
*   **Post:** Increment count. If negative (prior to increment), wake one waiter.

### 4.3 Condition Variables (`cond_t`)

Implements Mesa-style monitors.
*   **Wait:**
    1.  Enqueue self.
    2.  **Release Mutex** (Atomic-ish: We release and sleep. Because we are cooperative, no one can interrupt us between release and sleep).
    3.  `thread_schedule()`
    4.  **Re-acquire Mutex** upon waking.
*   **Signal/Broadcast:** Wakes one/all waiting threads.

### 4.4 Channels (`channel_t`)

Inspired by Go channels. Implemented as a thread-safe bounded buffer using our own Mutex and Condition Variables.
*   **Structure:** Circular buffer logic (`head`, `tail`).
*   **Synchronization:**
    *   `mutex` protects the buffer state.
    *   `not_full` cond var: Senders wait here if buffer full.
    *   `not_empty` cond var: Receivers wait here if buffer empty.
*   **Closing:** A `closed` flag allows graceful shutdown, waking all sleepers.

## 5. Part 3: Real-World Scenarios

### 5.1 Producer-Consumer
We implemented this twice:
1.  **With Semaphores (`pc_sem.c`):** Uses `empty_slots` and `full_slots` semaphores to coordinate production/consumption.
2.  **With Channels (`pc_chan.c`):** Simply uses `channel_send` and `channel_recv`. This demonstrated the power of higher-level abstractions; the code was significantly cleaner and easier to reason about logic.

### 5.2 Reader-Writer Lock (Writer Priority)

We implemented a **Writer-Priority** lock to prevent writer starvation.
*   **State:** Tracks `readers_active`, `writers_waiting`, `writer_active`.
*   **Logic:**
    *   **Reader Entry:** Must wait if `writer_active` **OR** `writers_waiting > 0`. This is key for writer priority.
    *   **Writer Entry:** Must wait if `writers_active` OR `readers_active > 0`.
    *   **Writer Exit:** Wakes other writers first. If none, wakes all readers (`cond_broadcast`).

## 6. Part 4: Thread-Safe File I/O

For this part, we simulated an asynchronous I/O pattern using processes and threads.
*   **Challenge:** In a 1:N model, a blocking `read()` blocks the whole process.
*   **Solution:** We created a Multi-process Producer-Consumer.
    *   **Process A (Producer):** Runs multiple threads writing to a pipe.
    *   **Process B (Consumer):** Runs multiple threads reading from the pipe.
    *   **Async-like Behavior:** Since they are separate processes, if the Consumer blocks on `read` (empty pipe), the Producer process can still run and write data. This achieves concurrency at the OS level while managing it with our user-level library within each process.

## 7. Build System

We modified the xv6 `Makefile` to link our library only to programs starting with `_t_` (e.g., `_t_pc_sem`). This prevents code bloat in standard xv6 utilities, avoiding the file system limit issues.

---
**Conclusion:**
This project successfully demonstrates the internals of a threading library. By implementing primitives from scratch, we gained deep insight into the cost of context switches, the necessity of atomic state management, and the elegance of cooperative scheduling.
