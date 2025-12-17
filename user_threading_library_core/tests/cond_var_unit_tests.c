#include "uthreads_test_suite.h"

static mutex_t m;

/* execution paths */
static cond_t c; // general condition variable for use with exec paths tests

static void cond_wait_while_unlocked_should_fail(void) {
    thread_init();
    mutex_init(&m);
    cond_init(&c);

    // if the mutex has just been initialized and is unlocked,
    // then no one is the owner. therefore this should fail the
    // first conditional check

    cond_wait(&c, &m);
}

static void cond_wait_while_not_owner_should_fail(void) {
    thread_init();
    mutex_init(&m);
    cond_init(&c);

    m.owner = (struct thread*) 0xdeadbeef;
    m.locked = 1;

    // call cond_wait without first locking the mutex
    cond_wait(&c, &m);
}

/* producer/consumer test w/ cond vars */

static cond_t not_empty;
static cond_t not_full;

static int buffer = 0;
static int has_item = 0;

/*
producer

Produces items 1 through 5.
Waits if the buffer is full (has_item == 1).
*/
static void *producer(void *arg) {
  for (int i = 1; i <= 5; i++) {
    mutex_lock(&m);

    // Wait while buffer is full
    while (has_item) {
      cond_wait(&not_full, &m);
    }

    // Produce item
    buffer = i;
    has_item = 1;

    printf(1, "produced %d\n", i);

    // Signal consumer that buffer is not empty
    cond_signal(&not_empty);
    mutex_unlock(&m);
  }
  return 0;
}

/*
consumer

Consumes items.
Waits if buffer is empty (!has_item).
*/
static void *consumer(void *arg) {
  for (int i = 1; i <= 5; i++) {
    mutex_lock(&m);

    // Wait while buffer is empty
    while (!has_item) {
      cond_wait(&not_empty, &m);
    }

    int from_buffer = buffer;
    has_item = 0;

    printf(1, "consumed %d\n", from_buffer);

    if (from_buffer != i) {
        printf(1, "[FAIL] (expected %d, received %d)\n", i, from_buffer);
        exit();
    }

    // Signal producer that buffer is not full
    cond_signal(&not_full);
    mutex_unlock(&m);
  }
  return 0;
}

static void cond_var_producer_consumer_ok(void) {
    thread_init();
    mutex_init(&m);
    cond_init(&not_empty);
    cond_init(&not_full);

    int producer_thread = thread_create(producer, 0);
    int consumer_thread = thread_create(consumer, 0);

    thread_join(producer_thread);
    thread_join(consumer_thread);
}

/* cond_var broadcast test */

static cond_t ready_cond;
static int ready = 0;
static int woke = 0;

// Each waiter sleeps until the shared predicate 'ready' flips to 1
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

static void cond_var_broadcast_ok(void) {
    thread_init();
    mutex_init(&m);
    cond_init(&ready_cond);

    // Create 3 worker threads
    int t1 = thread_create(waiter, 0);
    int t2 = thread_create(waiter, 0);
    int t3 = thread_create(waiter, 0);

    if (t1 < 0 || t2 < 0 || t3 < 0) {
        printf(1, "[FAIL] thread_create failed\n");
    }

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
        printf(1, "[FAIL] some waiters did not wake after broadcast (expected 3, got %d)\n", woke);
        exit();
    }
}

int main(void) {
    printf(1, "=== cond_var suite ====\n");
    run_expect_exit("cond_wait while unlocked should exit", cond_wait_while_unlocked_should_fail);
    run_expect_exit("cond_wait by not owner should exit", cond_wait_while_not_owner_should_fail);
    run_ok("cond_var producer consumer sequences correctly", cond_var_producer_consumer_ok);
    run_ok("cond_var broadcast wakes all waiters", cond_var_broadcast_ok);
    printf(1, "=== done: %d run, %d failed\n", tests_run, tests_failed);
    exit();
}
