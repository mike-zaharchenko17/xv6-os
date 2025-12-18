#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

static channel_t *ch;          // Pointer to the communication channel
static int producers_done = 0; // Counter for finished producers
static mutex_t done_lock;      // Mutex to protect access to producers_done

/* Producers send ITEMS_PER_PRODUCER integers; final producer closes channel */
static void *producer(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
    int val = id * 100 + i;
    // Send value to channel. channel_send handles all blocking/synchronization
    // internally.
    channel_send(ch, (void *)val);
    printf(1, "Producer %d: produced item %d\n", id, i);
  }

  // Checking if this is the last producer to finish
  mutex_lock(&done_lock);
  producers_done++;
  if (producers_done == NUM_PRODUCERS) {
    // Last producer closes the channel.
    // This causes any future sends to fail (not used here) and wakes up all
    // waiting receivers indicating that no more data will arrive.
    channel_close(ch);
  }
  mutex_unlock(&done_lock);
  return 0;
}

static void *consumer(void *arg) {
  int id = (int)arg;
  int received = 0;
  while (1) {
    void *data = 0;
    // Receive data from channel. Blocks if empty.
    // Returns < 0 if the channel is closed and empty.
    int rc = channel_recv(ch, &data);
    if (rc < 0) {
      break; // Channel closed and drained, time to exit
    }
    received++;
    printf(1, "Consumer %d: consumed item %d\n", id, (int)data);
  }
  printf(1, "Consumer %d: exiting after %d items\n", id, received);
  return 0;
}

int main(void) {
  thread_init();          // Initialize threading library
  mutex_init(&done_lock); // Initialize helper mutex
  ch = channel_create(5); // Create a channel with a buffer size of 5
  if (!ch) {
    printf(1, "pc_chan_test: failed to create channel\n");
    exit();
  }

  int producers[NUM_PRODUCERS];
  int consumers[NUM_CONSUMERS];

  // Spawn producers
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    producers[i] = thread_create(producer, (void *)i);
  }
  // Spawn consumers
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    consumers[i] = thread_create(consumer, (void *)i);
  }

  // Wait for all producers
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    thread_join(producers[i]);
  }
  // Wait for all consumers
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    thread_join(consumers[i]);
  }

  printf(1, "pc_chan_test: PASS (channel closed and drained)\n");
  exit();
}
