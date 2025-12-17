#include "types.h"
#include "user.h"
#include "uthreads.h"

static mutex_t m;
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

    // Signal producer that buffer is not full
    cond_signal(&not_full);
    mutex_unlock(&m);
  }
  return 0;
}

int main(void) {
  thread_init();
  mutex_init(&m);
  cond_init(&not_empty);
  cond_init(&not_full);

  int producer_thread = thread_create(producer, 0);
  int consumer_thread = thread_create(consumer, 0);

  thread_join(producer_thread);
  thread_join(consumer_thread);

  printf(1, "condvar_basic_test: PASS\n");
  exit();
}
