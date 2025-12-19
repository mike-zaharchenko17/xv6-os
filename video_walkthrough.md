# Video Walkthrough Script: xv6 User-Level Threading Library

**Target Audience:** Instructors and TAs.
**Goal:** Demonstrate functionalities AND justify key design decisions (the "Why").

---

## 1. Introduction (0:00 - 0:45)

**Action:** Open `user_threading_library_core/src/uthreads.h` on the left.
**Action:** Open `Final Project.md` on the right.

**Say:**
"Hello! This is [Your Name], presenting the xv6 User-Level Threading Library. This project implements an N:1 threading model.
**Why N:1?** Because it allows us to handle scheduling entirely in user space without kernel modifications, giving us fast context switches.

I will walk through the four parts, focusing on the *design decisions* that make the library robust."

---

## 2. Part 1: Threading Foundation (0:45 - 2:30)

**Action:** Open `uthreads.c` -> `thread_create`.
**Action:** Highlight the stack setup loop.

**Say:**
"First, **Thread Creation**. Creating a thread isn't just allocating memory; it's about fooling the CPU."

**Design Decision: Stack Spoofing**
"**Why did we manually push 0s and an address to the stack?**
Because our context switch routine (`thread_switch`) assumes it's returning from a function call. It *expects* to pop registers (EBP, EBX, ESI, EDI). If creation didn't mimic this layout, the first context switch to a new thread would crash the CPU. We 'spoof' a stack frame so the thread 'returns' safely into `thread_trampoline`."

**Action:** Open `thread_switch.S`.

**Say:**
"This is `thread_switch.S`. It swaps only the stack pointer (`%esp`)."

**Design Decision: Assembly vs C**
"**Why Assembly?**
C functions use the stack for their own variables. You cannot atomically swap the stack *underneath* a running C function without corrupting its local state. Only assembly allows us to control exactly which registers are touched during the swap."

**Action:** Run Terminal Command: `t_thread_unit_`

**Say:**
"Demo: Thread unit tests pass, proving our stack spoofing works."

---

## 3. Part 2: Synchronization Primitives (2:30 - 4:45)

### Mutexes & Handoff
**Action:** Open `uthreads.c` -> `mutex_unlock`.

**Say:**
"Next, Synchronization. The most critical decision here was in `mutex_unlock`."

**Design Decision: Direct Handoff (The "Anti-Barging" Lock)**
"**Why did we implement ownership handoff?**
Standard mutexes unlock the lock and wake a sleeper. This creates a race (or 'barging') where a *new* thread on another core might steal the lock before the woken thread runs.
**Our Solution:** In `mutex_unlock`, we do **not** set `locked=0`. We find a waiter, assign it as the new owner, and keep `locked=1`.
**Why?** This guarantees fairness. The woken thread is *guaranteed* to be the next owner. No spinning, no starvation."

### Channels (Extra Credit)
**Action:** Open `uthreads.c` -> `channel_send` and `channel_recv`.

**Say:**
"I also implemented **Channels**."

**Design Decision: Abstraction Layer**
"**Why Channels?**
Using raw Mutexes/CVs is error-prone (forgetting to lock, lost wakeups). Channels encapsulate this complexity.
**The Design:** Internally, it's a fixed-size ring buffer. `channel_send` automatically blocks on `not_full`, and `channel_recv` blocks on `not_empty`. This ensures safety by design—users *cannot* misuse the synchronization logic."

**Action:** Run Terminal Command: `t_channel_test`

**Say:**
"Demo: Channel tests pass, showing correct blocking behavior."

---

## 4. Part 3: Real-World Problems (4:45 - 6:45)

### Reader-Writer Lock (Writer Priority)
**Action:** Open `examples/rw_lock.c` -> `reader_lock`.

**Say:**
"For the Reader-Writer lock, we had to choose a policy."

**Design Decision: Writer Priority**
"**Why Writer Priority?**
In many systems, reads are frequent and writes are rare. If we allowed readers to enter whenever the lock is free of *active* writers, a continuous stream of readers would starve the writer forever.
**Our Fix:** A reader *must check* `writers_waiting`. If *any* writer is waiting, the reader blocks. This prioritizes the 'starving' class (writers) over the 'abundant' class (readers)."

**Action:** Run Terminal Command: `t_rw_lock`

**Say:**
"output shows batches of reads stopping specifically to let a writer in."

---

## 5. Part 4: Thread-Safe File I/O (6:45 - End)

**Action:** Open `examples/thread_safe_file_io.c`.

**Say:**
"Finally, Thread-Safe I/O. This was the hardest challenge."

**Design Decision: Multi-Process Architecture**
"**Why Multi-Process?**
We are in a User-Level Threading library. The OS kernel sees us as **one single process**. If one thread calls `read()`, the **kernel** puts the entire process to sleep. All threads stop.
**The Solution:** We *must* use multiple processes to get OS-level concurrency.
We fork a 'Producer Process' and a 'Consumer Process'.
**Why?** Now, if the Consumer blocks on `read()`, the OS scheduler can still run the Producer Process. This simulates async I/O without kernel support."

**Action:** Run Terminal Command: `t_thread_safe_`

**Say:**
"Demo: The Producer generates data even while the Consumer waits, proving we broke the single-process blocking limitation."

---

## 6. Closing

**Say:**
"To summarize: We didn't just write code that compiles. We made specific architectural choices—Stack Spoofing for context safety, Mutex Handoff for fairness, Writer Priority for liveness, and Multi-Processing for non-blocking I/O. Thank you."
