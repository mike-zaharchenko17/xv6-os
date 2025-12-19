# Video Walkthrough Script: Advanced Concurrency

**Target Audience:** Beginners who need to understand how Channels and RW Locks work under the hood.
**Goal:** Explain *why* we built things this way and prove it works.

---

## 1. Introduction & Setup (0:00 - 0:30)

**Action:** Open `user_threading_library_core/src/uthreads.c` on the left.
**Action:** Open a terminal on the right.

**Say:**
"Hi everyone. Today we're looking at the advanced concurrency primitives we added to our xv6 threading library: **Channels** and **Reader-Writer Locks**. We built these on top of the basic Mutexes and Condition Variables we saw earlier."

---

## 2. Deep Dive: Channels (0:30 - 2:30)

**Action:** Scroll to `channel_create` struct definition in `uthreads.c` (around line 460).

**Say:**
"First, **Channels**. Based on Go's channels, these are the safest way for threads to share data. Instead of sharing memory and fighting over locks, threads send messages."

**Action:** Highlight the `channel_t` struct (mentally or with mouse).
**Say:**
"Under the hood, a channel is just a **Ring Buffer** protected by a **Mutex** and two **Condition Variables**:
1. `not_full`: Where Senders wait if the buffer is full.
2. `not_empty`: Where Receivers wait if the buffer is empty."

**Action:** Scroll to `channel_send` (around line 492).
**Say:**
"Look at `channel_send`. It's a perfect example of a monitor pattern:
1. **Lock** the mutex.
2. **Loop** while the buffer is full, resolving the `not_full` condition.
3. Once there's space, we write to the buffer.
4. Finally, we **Signal** `not_empty` to wake up any sleeping receivers."

**Action:** Scroll to `channel_recv` (around line 524).
**Say:**
"Receive is the mirror image. We Lock, wait on `not_empty`, read the data, and then signal `not_full` to tell senders that space just opened up."

---

## 3. The Producer-Consumer Problem (2:30 - 3:30)

**Action:** Open `user_threading_library_core/examples/pc_chan.c`.

**Say:**
"To test this, we built a Producer-Consumer solution. Traditionally, this requires complex semaphore math. With channels, it becomes trivial."

**Action:** Highlight the `producer` function loop.
**Say:**
"The producer just calls `channel_send`. If the channel is full, it sleeps automatically. No manual semaphore management needed."

**Action:** Highlight the `consumer` function loop.
**Say:**
"The consumer just calls `channel_recv`. If the channel is empty, it sleeps. If the producer closes the channel, `recv` returns `-1`, and the consumer exits cleanly. This elegant shutdown is a huge advantage over semaphores."

---

## 4. Deep Dive: Writer-Priority RW Lock (3:30 - 5:00)

**Action:** Open `user_threading_library_core/examples/rw_lock.c`.

**Say:**
"Next, the **Reader-Writer Lock**. This allows multiple threads to read shared data simultaneously, but requires exclusive access for writing."

**Action:** Scroll to `reader_lock` (around line 24).
**Say:**
"The critical feature here is **Writer Priority**. In a standard implementation, a constant stream of Readers could starve a Writer, preventing it from ever running. We fixed that."

**Action:** Highlight the `while` loop condition in `reader_lock`.
```c
while (s->writer_active || s->writers_waiting > 0)
```
**Say:**
"Look at this line. A reader waits if a writer is active, OR if `writers_waiting > 0`. This means if a Writer *wants* to enter, new Readers must wait, clearing the path for the Writer."

**Action:** Scroll to `writer_unlock` (around line 69).
**Say:**
"When a writer finishes, it checks `writers_waiting`. It prefers to wake up another Writer (maintaining the writer streak) before waking up all the Readers."

---

## 5. Live Demo (5:00 - End)

**Action:** Switch to Terminal.
**Action:** Run `make qemu`.

**Say:**
"Let's see it in action."

### Demo 1: Channels
**Command:** `t_pc_chan`
**Say:**
"Running the Producer-Consumer channel test..."
*(Point to output)*
"See how Producers (P) and Consumers (C) interleave perfectly. P fills the buffer, then C drains it. Finally, 'PASS' confirms the clean shutdown."

### Demo 2: RW Lock
**Command:** `t_rw_lock`
**Say:**
"Now the Reader-Writer lock..."
*(Point to output)*
"Watch closely. You'll see batches of Readers running together. But once a Writer requests the lock, you won't see new Readers starting until that Writer has finished. That proves our Writer Priority works."

**Conclusion:**
"And that's how we implemented robust concurrency primitives in xv6. Thanks for listening!"
