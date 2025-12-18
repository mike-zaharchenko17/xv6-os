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
    exit();
  }

  // Verify that all other thread slots (1 to 31) are cleanly initialized.
  for (int i = 1; i < MAX_THREADS; i++) {
    if (!(threads[i].tid == -1) ||          // Should have no ID
        !(threads[i].tstate == T_UNUSED) || // Should be unused
        !(threads[i].stack == 0) || // Should have no stack allocated yet
        !(threads[i].sp == 0) || !(threads[i].start_routine == 0) ||
        !(threads[i].arg == 0) || !(threads[i].retval == 0) ||
        !(threads[i].joiner_tid == -1) || !(threads[i].qnext == 0)) {
      printf(1, "[FAIL] unexpected values in thread at idx %d\n", i);
      exit();
    }
  }
}

/* thread_self_ok */ 

static void thread_self_ok(void) {
  thread_init();

  current_thread->tid = 1234;

  if (thread_self() != 1234) {
    printf(1, "[FAIL] thread_self did not produce expected value (expected %d)\n", 1234);
    exit();
  }
}

/* thread_create_ok */

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

Tests the thread joining mechanism (blocking wait). happy path
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

/* thread_join failure path tests */

static void thread_self_join_fail_ok(void) {
  thread_init();
  void *r = thread_join(thread_self());

  if (r != 0) {
    printf(1, "[FAIL]: expected 0 for self-join");
    exit();
  }
}

static void thread_invalid_tid_join_fail_ok(void) {
  thread_init();
  void *r = thread_join(9999);

  if (r != 0) {
    printf(1, "[FAIL]: expected 0 for invalid tid join");
    exit();
  }
}

/* thread_join enforce single joiner tests */

static volatile int j1_done = 0;
static volatile int j2_done = 0;
static int target_tid = -1;

static void *target_fn(void *arg) {
  // this will be what joiners want to join
  thread_yield();
  thread_yield();
  return (void*)0x55;
}

static void *joiner_1(void *arg) {
  void *r = thread_join(target_tid);
  if (r == (void*)0x55) {
    j1_done = 1;
  }
  return 0;
}

static void *joiner_2(void *arg) {
  void *r = thread_join(target_tid);
  if (r == 0) {
    j2_done = 1;
  }
  return 0;
}

static void thread_join_single_joiner_ok(void) {
  thread_init();

  target_tid = thread_create(target_fn, 0);

  int j1 = thread_create(joiner_1, 0);
  int j2 = thread_create(joiner_2, 0);

  for (int i = 0; i < 10; i++) {
    // yielding to let joiners contend
    thread_yield(); 
  } 

  thread_join(j1);
  thread_join(j2);

  // both should finish- "success" is that j1 gets the retval
  // and j2 gets a zero (failure). If this condition is met, 
  // then we set both to one, so j1_done + j2_done should == 2
  if (j1_done + j2_done != 2) {
    printf(1, "[FAIL] expected one join success and one join failure (got %d)\n");
  }
}

/*
thread_create_unique_tid

uses previously-defined join_worker_fast's (since they're very generic)

tests that thread_create returns unique tids and increments next_tid

*/
static void thread_create_unique_tid_ok(void) {
  thread_init();
  int t1 = thread_create(join_worker_fast, 0);
  int t2 = thread_create(join_worker_fast, 0);
  int t3 = thread_create(join_worker_fast, 0);

  if (!(t1 > 0 && t1 > 0 && t2 > 0)) {
    printf(1,"[FAIL] bad tids- zero or neg\n"); 
    exit();
  }

  if (t1 == t2 || t1 == t3 || t2 == t3) { 
    printf(1,"[FAIL] duplicate tid; not incrementing\n"); 
    exit(); 
  }
}

/*

thread_slot_reuse_ok

verifies that slot becomes T_UNUSED and can be reused; no leaks given
that we're freeing the stack

*/

static int find_slot_idx_by_tid(int tid) {
  for (int i = 0; i < MAX_THREADS; i++) {
    if (threads[i].tid == tid) {
      return i;
    }
  }
  return -1;
}

static void thread_slot_reuse_ok(void) {
  thread_init();

  int t1 = thread_create(join_worker_fast, 0);

  thread_yield();

  int t1_slot = find_slot_idx_by_tid(t1);

  thread_join(t1);

  if (threads[t1_slot].tstate != T_UNUSED) {
    printf(1, "[FAIL] slot %d not UNUSED after join\n", t1_slot);
    exit();
  }
  if (threads[t1_slot].tid != -1) {
    printf(1, "[FAIL] slot %d tid not reset after join\n", t1_slot);
    exit();
  }
  if (threads[t1_slot].stack != 0) {
    printf(1, "[FAIL] slot %d stack not freed after join\n", t1_slot);
    exit();
  }
  if (threads[t1_slot].sp != 0) {
    printf(1, "[FAIL] slot %d sp not reset after join\n", t1_slot);
    exit();
  }

  int t2 = thread_create(join_worker_fast, 0);

  int t2_slot = find_slot_idx_by_tid(t2);

  if (t1_slot != t2_slot) {
    printf(1, "[FAIL] slot indices did not match (expected [1, 1], got [%d, %d])\n", t1_slot, t2_slot);
    exit();
  }
}

static void thread_create_limit_ok(void) {
  thread_init();
  int tids[MAX_THREADS];

  for (int i = 1; i < MAX_THREADS; i++) {
    tids[i] = thread_create(join_worker_slow, 0);
    if (tids[i] < 0) { 
      printf(1, "[FAIL] early failure at %d\n", i); 
      exit(); 
    }
  }

  int extra = thread_create(join_worker_slow, 0);
  if (extra != -1) { 
    printf(1, "[FAIL] expected -1 when full\n");
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

/*

thread_schedule_no_runnable_should_exit

tests that thread_schedule (which is called from thread_exit)
correctly exits when there are no threads available to run

*/
static void thread_schedule_no_runnable_should_exit(void) {
  thread_init();
  thread_exit(0);
  printf(1, "[FAIL] schedule returned unexpectedly\n");
  exit();
}

int main(void) {
  printf(1, "=== thread suite ====\n");

  run_ok("thread_init ok", thread_init_ok);

  run_ok("thread_create ok", thread_create_ok);

  run_ok("thread_self ok", thread_self_ok);

  run_ok("thread_join ok", thread_join_ok);

  run_ok("thread_join returns 0 on self join", thread_self_join_fail_ok);

  run_ok("thread_join returns 0 on invalid tid join", thread_invalid_tid_join_fail_ok);

  run_ok("thread_join enforces single joiner", thread_join_single_joiner_ok);

  run_ok("thread_create starts from 1 (not 0) and increments", thread_create_unique_tid_ok);

  run_ok("threads can reuse slots after join", thread_slot_reuse_ok);

  run_ok("thread_create returns -1 if limit exceeded", thread_create_limit_ok);

  run_ok("thread_schedule ok", thread_schedule_empty_ok);

  run_expect_exit("thread_schedule exits when no threads available", thread_schedule_no_runnable_should_exit);

  printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
  exit();
}