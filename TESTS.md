# Threading Library Tests

This document lists the user-level threading tests, what they cover, and how to build and run them inside xv6. xv6 uses 14-character filenames; the binary names are truncated accordingly—both forms are included below.

## Build (compiles everything)
1. From the repo root, build the filesystem image (this also compiles all user tests):
   ```sh
   make fs.img
   ```
2. Launch xv6 in a terminal:
   ```sh
   make qemu-nox
   ```
   (Use `make qemu` if you prefer the graphical console.)

## Running tests inside xv6
At the xv6 shell prompt (`$`), run each test by its truncated name (shown in the table). Example:
```
$ t_channel_test
```
Exit xv6 with `Ctrl-a x`.

## Test catalog
| Source file (full) | xv6 binary name | Purpose |
| --- | --- | --- |
| user_threading_library_core/tests/cond_var_producer_consumer_test.c | t_cond_var_pro | Producer/consumer using condition variables + mutex (bounded buffer). |
| user_threading_library_core/tests/cond_var_broadcast_test.c | t_cond_var_bro | Broadcast wakes all waiters on a condition variable. |
| user_threading_library_core/tests/mutex_unit_tests.c | t_mutex_unit_t | Basic mutex correctness (lock/unlock, ownership checks). |
| user_threading_library_core/tests/semaphore_unit_tests.c | t_semaphore_un | Semaphore wait/post behavior. |
| user_threading_library_core/tests/thread_unit_tests.c | t_thread_unit_ | Core threading lifecycle (create/join/yield/exit). |
| user_threading_library_core/tests/channel_tests.c | t_channel_test | Channel basics: send/recv ordering, full-buffer blocking, close wakeups. |
| user_threading_library_core/tests/pc_sem_test.c | t_pc_sem_test | Producer/consumer (3 producers×10 items, 2 consumers, buffer 5) using semaphores + mutex with sentinels for shutdown. |
| user_threading_library_core/tests/pc_chan_test.c | t_pc_chan_tes | Same producer/consumer workload using channel_t; last producer closes channel to end consumers. |
| user_threading_library_core/tests/rw_lock_test.c | t_rw_lock_tes | Writer-priority reader/writer lock; multiple readers/writers contend, no new readers admitted while writers wait. |

## Notes
- All user tests are statically linked and copied into `fs.img` by `make fs.img`; no extra compile steps are needed beyond `make fs.img`.
- If you add or modify tests, rerun `make fs.img` before booting xv6 to ensure the new binaries are on the disk image.
- Use `ls` inside xv6 to see the truncated names if you forget them. ***


# Questions to Conider. 
- How will you coordinate when all producers have finished?
> Coordinate end-of-production by counting finished producers under the buffer mutex; the last producer injects one sentinel item per consumer (see user_threading_library_core/tests/pc_sem_test.c).
- How will consumers know there's no more work?
> Consumers detect no more work by dequeuing the sentinel; each consumer re-enqueues the sentinel for others, then exits.
- What should each semaphore be initialized to?
> Semaphore init: empty_slots starts at buffer capacity (all slots free), full_slots starts at 0 (no items yet).




- How do you prevent new readers from starting when a writer is waiting? 
>Block new readers when any writer is waiting or active: gate reader_lock on (writer_active || writers_waiting > 0) so arriving readers cond_wait instead of starving writers (user_threading_library_core/tests/rw_lock_test.c).
- When should you wake up waiting readers vs. waiting writers? 
>Wake choice: on writer_unlock, if writers_waiting > 0 signal a writer; otherwise broadcast to all readers. On reader_unlock, if this was the last reader and writers are waiting, signal a writer.
- What happens if multiple writers are waiting? 
>Multiple waiting writers: track writers_waiting; each release signals one writer, which decrements the count and marks writer_active, serializing writers while preventing reader starvation.
Last-reader handoff: in reader_unlock, when readers_active drops to 0 and writers_waiting > 0, signal writers_ok so a queued writer runs next.
- How do you ensure the last reader wakes up a waiting writer?
>Last-reader handoff: in reader_unlock, when readers_active drops to 0 and writers_waiting > 0, signal writers_ok so a queued writer runs next.


