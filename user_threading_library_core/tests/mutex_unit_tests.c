#include "uthreads_test_suite.h"

static mutex_t m;

/* execution paths */

/*
mutex_self_lock_should_exit

Tests that recursively locking the same mutex from the same thread causes an
error (deadlock prevention), instead of hanging.
*/
static void mutex_self_lock_should_exit(void) {
  thread_init();
  mutex_init(&m);

  mutex_lock(&m);
  mutex_lock(&m); // should print a deadlock message and exit
}

/*
mutex_unlock_not_by_owner_should_exit

Tests that a thread cannot unlock a mutex it does not own.
*/
static void mutex_unlock_not_by_owner_should_exit(void) {
  thread_init();
  mutex_init(&m);

  m.locked = 1;
  m.owner = (struct thread *)0xdeadbeef; // Fake owner

  mutex_unlock(&m); // Current thread tries to unlock -> Should exit
}

static void mutex_unlock_by_owner_should_run(void) {
  thread_init();
  mutex_init(&m);
  mutex_lock(&m);
  mutex_unlock(&m);
}

static volatile int phase = 0;
static volatile int b_entered = 0;

static struct thread *a_thread_ptr = 0;
static struct thread *b_thread_ptr = 0;

static void *thread_b(void *arg) {
  // record B’s thread pointer (white-box)
  b_thread_ptr = current_thread;

  phase = 1;
  mutex_lock(&m);

  b_entered = 1;
  phase = 3;

  mutex_unlock(&m);
  return 0;
}

static void *thread_a(void *arg) {
  a_thread_ptr = current_thread;

  mutex_lock(&m);

  int tidb = thread_create(thread_b, 0);

  // ensure B starts
  while (phase < 1) {
    thread_yield();
  }

  // give B a chance to actually run mutex_lock() and go to sleep.
  // B must enqueue + sleep in mutex_lock.
  thread_yield();

  // B should be waiting (sleeping in mutex_lock).

  // now unlock and hand off to waiter
  mutex_unlock(&m);

  if (m.locked != 1) {
    printf(1, "[FAIL] expected m.locked==1 after handoff\n");
    exit();
  }
  if (m.owner == 0) {
    printf(1, "[FAIL] expected m.owner!=NULL after handoff\n");
    exit();
  }
  if (m.owner == a_thread_ptr) {
    printf(1, "[FAIL]: expected ownership transferred away from A\n");
    exit();
  }

  // if we managed to capture b_thread_ptr, it should be exactly the owner.
  if (b_thread_ptr != 0 && m.owner != b_thread_ptr) {
    printf(1, "[FAIL] expected owner to be B after handoff\n");
    exit();
  }

  // let B run and enter CS
  while (b_entered == 0) thread_yield();

  thread_join(tidb);
  return 0;
}

static void mutex_unlock_handoff_ok(void) {
  thread_init();
  mutex_init(&m);

  // reset shared flags
  phase = 0;
  b_entered = 0;
  a_thread_ptr = 0;
  b_thread_ptr = 0;

  int tida = thread_create(thread_a, 0);
  thread_join(tida);
}


/* counter setup and test */
static volatile int counter = 0;

static void *counter_worker(void *arg) {
  for (int i = 0; i < 1000; i++) {
    mutex_lock(&m); // ENTER CRITICAL SECTION

    // begin critical section
    int tmp = counter;
    // force a context switch while holding lock using yield.
    // If mutex is broken, another thread will run here and read the OLD value
    // of 'counter'
    thread_yield();
    counter = tmp + 1;
    // end critical section

    mutex_unlock(&m); // EXIT CRITICAL SECTION

    // optional: encourage mixing
    if ((i % 50) == 0)
      thread_yield();
  }

  return 0;
}

/*
mutex_protects_counter_ok

Critical concurrency test.
Spawns 4 threads. Each increments 'counter' 1000 times.
If mutual exclusion works, final result must be 4000.
*/
static void mutex_protects_counter_ok(void) {
  thread_init();
  mutex_init(&m);

  counter = 0;

  int t1 = thread_create(counter_worker, 0);
  int t2 = thread_create(counter_worker, 0);
  int t3 = thread_create(counter_worker, 0);
  int t4 = thread_create(counter_worker, 0);

  thread_join(t1);
  thread_join(t2);
  thread_join(t3);
  thread_join(t4);

  if (counter != 4 * 1000) {
    printf(1, "[FAIL] mutex counter wrong: %d\n", counter);
    exit();
  }
}

static void *counter_worker_no_mutex(void *arg) {
  for (int i = 0; i < 1000; i++) {
    // begin critical section (unprotected)
    int tmp = counter;
    // force a context switch while holding lock
    thread_yield();
    counter = tmp + 1;
    // end unprotected critical section

    // optional: encourage mixing
    if ((i % 50) == 0)
      thread_yield();
  }

  return 0;
}

static void unprotected_counter_test_fails(void) {
  thread_init();
  mutex_init(&m);

  int expected = 4000;
  counter = 0;

  int t1 = thread_create(counter_worker_no_mutex, 0);
  int t2 = thread_create(counter_worker_no_mutex, 0);
  int t3 = thread_create(counter_worker_no_mutex, 0);
  int t4 = thread_create(counter_worker_no_mutex, 0);

  thread_join(t1);
  thread_join(t2);
  thread_join(t3);
  thread_join(t4);

  if (counter != 4 * 1000) {
    printf(1, "[PASS] mutex counter wrong (expected: %d, actual: %d)\n",
           expected, counter);
  } else {
    printf(1, "[FAIL] mutex counter correct");
  }

  exit();
}

int main(void) {
  printf(1, "=== mutex suite ====\n");

  run_expect_exit("mutex self lock exits", mutex_self_lock_should_exit);

  run_expect_exit("mutex unlock by not owner exits",
                  mutex_unlock_not_by_owner_should_exit);

  run_ok("mutex should hand off to waiting thread on unlock", mutex_unlock_handoff_ok);

  run_ok("mutex lock/unlock by owner runs", mutex_unlock_by_owner_should_run);

  run_ok("mutex correctly protects counter", mutex_protects_counter_ok);

  // run ok because you're not gonna get kicked out
  run_ok("counter test fails without mutex", unprotected_counter_test_fails);

  printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
  exit();
}
