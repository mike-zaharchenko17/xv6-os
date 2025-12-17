#include "types.h"
#include "user.h"
#include "uthreads.h"

static mutex_t m;
static cond_t ready_cond;
static int ready = 0;
static int woke = 0;

static void fail(const char *msg) {
  printf(1, "cond_broadcast_test: FAIL: %s\n", msg);
  exit();
}

/* Each waiter sleeps until the shared predicate 'ready' flips to 1 */
static void *waiter(void *arg) {
  mutex_lock(&m);
  // Standard pattern: Loop while condition is false
  while (!ready) {
    cond_wait(&ready_cond, &m);
  }
  woke++;
  mutex_unlock(&m);
  return 0;
}

int main(void) {
  thread_init();
  mutex_init(&m);
  cond_init(&ready_cond);

  // Create 3 worker threads
  int t1 = thread_create(waiter, 0);
  int t2 = thread_create(waiter, 0);
  int t3 = thread_create(waiter, 0);
  if (t1 < 0 || t2 < 0 || t3 < 0)
    fail("thread_create failed");

  // Let all waiters block on the condition variable.
  thread_yield();

  // Main thread acquires lock, sets condition, and wakes EVERYONE.
  mutex_lock(&m);
  ready = 1;
  cond_broadcast(&ready_cond);
  mutex_unlock(&m);

  thread_join(t1);
  thread_join(t2);
  thread_join(t3);

  // Verify all threads woke up
  if (woke != 3) {
    fail("not all waiters woke after broadcast");
  }

  printf(1, "cond_broadcast_test: PASS\n");
  exit();
}
