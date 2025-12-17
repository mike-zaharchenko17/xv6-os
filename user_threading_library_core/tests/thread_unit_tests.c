#include "uthreads_test_suite.h"

/*
thread_init_ok

Tests the initialization of the threading library.
Verifies that the main thread is correctly set up as thread 0.
*/
static void thread_init_ok(void) {
  thread_init();

  // The main program itself becomes thread 0.
  // Verify that the 'threads' array at index 0 is indeed the current running
  // thread.
  if (&threads[0] != current_thread) {
    printf(1, "[FAIL] thread at idx 0 and current thread tid did not match\n");
    exit();
  }

  // Verify the Thread ID (TID) is set to 0
  if (current_thread->tid != 0) {
    printf(1, "[FAIL] current thread tid is incorrect\n");
  }

  // Verify that all other thread slots (1 to 31) are cleanly initialized.
  for (int i = 1; i < MAX_THREADS; i++) {
    if (!(threads[i].tid == -1) ||          // Should have no ID
        !(threads[i].tstate == T_UNUSED) || // Should be unused
        !(threads[i].stack == 0) || // Should have no stack allocated yet
        !(threads[i].sp == 0) || !(threads[i].start_routine == 0) ||
        !(threads[i].arg == 0) || !(threads[i].retval == 0) ||
        !(threads[i].joiner_tid == -1) || !(threads[i].qnext == 0)) {
      printf(1, "[FAIL] unexpected values in thread at idx %d\n", 1);
      exit();
    }
  }
}

static void *worker1(void *arg) {
  for (int i = 0; i < 3; i++) {
    printf(1, "worker1 %d\n", i);
    thread_yield(); // Voluntarily yield CPU
  }
  return (void *)0xdeadbeef;
}

static void *worker2(void *arg) {
  for (int i = 0; i < 3; i++) {
    printf(1, "worker2 %d\n", i);
    thread_yield();
  }
  return (void *)0xdeadbeef;
}

static void *worker3(void *arg) {
  for (int i = 0; i < 3; i++) {
    printf(1, "worker3 %d\n", i);
    thread_yield();
  }
  return (void *)0xdeadbeef;
}

/*
thread_create_ok

Tests creation of multiple threads and their scheduling.
Since threads call yield(), we expect some interleaving.
*/
static void thread_create_ok(void) {
  thread_init();

  thread_create(worker1, 0);
  thread_create(worker2, 0);
  thread_create(worker3, 0);

  // expect roughly interleaving output
  for (int i = 0; i < 3; i++) {
    printf(1, "main %d\n", i);
    thread_yield();
  }
}

static void *join_worker_slow(void *arg) {
  // yield a couple times so main has a chance to try joining while we're alive
  thread_yield();
  thread_yield();
  return (void *)0x11111111;
}

static void *join_worker_fast(void *arg) { return (void *)0x22222222; }

/*
thread_join_ok

Tests the thread joining mechanism (blocking wait).
*/
static void thread_join_ok(void) {
  thread_init();

  // Test joining a thread that is still running (Slow Worker)
  int tid1 = thread_create(join_worker_slow, 0);

  // try joining immediately; should block until worker_slow returns.
  void *r1 = thread_join(tid1);

  // Verify return value
  if (r1 != (void *)0x11111111) {
    printf(1, "[FAIL] join_worker_slow: BAD retval %p\n", r1);
    exit();
  }

  // Test joining a thread that has already exited (Fast Worker)
  int tid2 = thread_create(join_worker_fast, 0);

  // let worker_fast run and exit first
  thread_yield();

  // now join should return immediately (already zombie)
  void *r2 = thread_join(tid2);
  if (r2 != (void *)0x22222222) {
    printf(1, "[FAIL] join_worker_fast: BAD retval %p\n", r2);
    exit();
  }
}

/*
thread_schedule_empty_ok

Tests that thread_yield works correctly even when the calling thread
is the ONLY thread in the system. It should simply return to itself.
*/
static void thread_schedule_empty_ok(void) {
  char buf[2];

  thread_init();

  buf[0] = 'a';

  thread_yield(); // Should return here immediately

  buf[1] = 'b';

  if (buf[0] != 'a' || buf[1] != 'b') {
    printf(1, "[FAIL] did not correctly update in-memory buffer\n");
    exit();
  }

  printf(1, "buf: %s\n", buf); // expect "ab?d" unless you set buf[2]
}

int main(void) {
  printf(1, "=== thread suite ====\n");

  run_ok("thread_init ok", thread_init_ok);

  run_ok("thread_create ok", thread_create_ok);

  run_ok("thread_join ok", thread_join_ok);

  run_ok("thread_schedule ok", thread_schedule_empty_ok);

  printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
  exit();
}