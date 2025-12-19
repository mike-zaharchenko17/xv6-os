# Video Walkthrough Guide: Channels & Advanced Concurrency

## Goals
- Explain how higher-level constructs (channels, RW lock) are built atop mutex/cond/sem.
- Show blocking/wakeup behavior live via the provided tests.
- Keep the audience oriented with “what is blocked, what wakes it” at every step.

## Setup for the demo
- From repo root: `make fs.img` then `make qemu-nox` (or `make qemu`).
- Inside xv6, binaries are truncated to 14 chars. Use:
  - Channels unit tests: `t_channel_test`
  - Producer/consumer (mutex+sem baseline): `t_producer_con`
  - Producer/consumer (sem/poison-pill): `t_pc_sem`
  - Producer/consumer (channel-based): `t_pc_chan`
  - Writer-priority RW lock: `t_rw_lock`
- Each test prints a `PASS` line on success; use `Ctrl-a x` to quit QEMU.
- If you edit code/tests, rerun `make fs.img` before launching QEMU.

## Pre-demo checklist
- Open side-by-side: `uthreads.c` (channel impl), `channel_tests.c`, `pc_sem.c`, `pc_chan.c`, `rw_lock.c`.
- Remind yourself: cooperative threads only switch on yield/wait; cond_wait releases lock atomically.
- Decide which tests to run live (minimal: `t_channel_test`, `t_rw_lock`; optional: `t_pc_sem`, `t_pc_chan`).

## Suggested narrative flow
1) Remind audience of cooperative user-level threads: no preemption, so blocking primitives park threads explicitly.
2) Show how primitives from Part 2 (mutex/cond/sem) get composed into higher-level patterns (channels, PC workflows, RW lock).
3) Run the matching test after explaining each code path.

## Section: Channels (Go-style)
- Code: `user_threading_library_core/src/uthreads.c` (`channel_create/send/recv/close`).
  - `channel_create`: ring buffer allocation + init of `lock`, `not_empty`, `not_full`, and indices (`head/tail/count/closed`).
  - `channel_send`: lock, while `count==capacity` wait on `not_full` (cond_wait releases lock); bail out if `closed`; enqueue at `tail`, `cond_signal(not_empty)`.
  - `channel_recv`: lock, while `count==0` and not `closed` wait on `not_empty`; if closed+empty return `-1`; dequeue at `head`, `cond_signal(not_full)`.
  - `channel_close`: set `closed=1`, broadcast both cond vars to wake all blocked senders/receivers.
- Tests: `user_threading_library_core/tests/channel_tests.c`.
  - Test 1 (`test_basic_send_recv`): start receiver that parks on empty; sender delivers `42`; join.
  - Test 2 (`test_block_on_full_then_recv`): cap-1 channel; pre-fill; launch sender that must block; `thread_yield` lets it sleep; recv frees space → sender completes → second recv gets sender payload.
  - Test 3 (`test_close_behavior`): waiter blocks on empty; `channel_close` wakes with `-1`; later sends/recvs after close fail.
- Live demo script:
  - Run `t_channel_test`.
  - Call out when sender blocks (after first send) and when receiver unblocks.
  - Highlight PASS line and the sequence showing unblock after `channel_close`.

## Section: Advanced Concurrency Problems
- Baseline producer/consumer with semaphores: `user_threading_library_core/examples/producer_consumer_problem.c`.
  - Mechanics: `empty` and `full` semaphores bound a size-5 buffer; mutex guards head/tail; poison pills (-1) stop consumers.
  - Talk track: “Producer waits on empty → lock → enqueue → unlock → post full. Consumer waits on full → lock → dequeue → unlock → post empty. Poison pill ends consumers.”
  - Demo (optional): `t_producer_con` to show semaphore gating and poison-pill exit.
- Producer/consumer with semaphores + sentinel: `user_threading_library_core/examples/pc_sem.c`.
  - Mechanics: `empty_slots/full_slots` enforce capacity; mutex guards buffer. Last producer injects `SENTINEL` per consumer. Consumers requeue sentinel to ensure every consumer sees it.
  - Talk track: emphasize why re-queueing sentinel prevents only-one-consumer exit.
  - Demo: run `t_pc_sem`; point to logs where consumers announce sentinel exit.
- Producer/consumer using channels: `user_threading_library_core/examples/pc_chan.c`.
  - Mechanics: channel replaces sem/mutex bookkeeping; `done_lock` only tracks how many producers finished; final producer calls `channel_close`.
  - Talk track: contrast simplicity vs semaphore version; `channel_recv` returns `<0` to signal drain+close.
  - Demo: run `t_pc_chan`; highlight consumer exit after close.
- Writer-priority reader/writer lock: `user_threading_library_core/examples/rw_lock.c`.
  - State: `readers_active`, `writers_waiting`, `writer_active` under one mutex with `readers_ok`/`writers_ok` cond vars.
  - `reader_lock`: blocks if writer active **or waiting** → enforces writer priority.
  - `writer_lock`: increments `writers_waiting`, waits for zero readers/writers, then sets `writer_active`.
  - `writer_unlock`: signals a waiting writer first; if none, broadcasts to all readers.
  - Demo: run `t_rw_lock`; call out when readers defer because `writers_waiting > 0` and when writers wake readers afterward.

## How to present the tests
- Before each run, restate the sync story: which condition blocks, who signals.
- Run the binary, let output scroll; verbally mark when threads block/unblock.
- After output stops, point at the PASS line and one or two key ordering lines that prove the policy (e.g., sender blocked until recv frees space; writer runs before queued readers).
- If time is short, run `t_channel_test` and `t_rw_lock` as the minimum; add `t_pc_sem` vs `t_pc_chan` if you want to contrast semaphore vs channel shutdown behavior.***
