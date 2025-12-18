#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)
#define BUF_CAP 5
#define SENTINEL -1

static int buffer[BUF_CAP]; // Shared circular buffer
static int head = 0;        // Index to remove items from
static int tail = 0;        // Index to insert items at
static mutex_t
    buf_lock; // Mutex to protect buffer access (head, tail, buffer array)
static sem_t empty_slots; // Semaphore to track available space in buffer
                          // (initialized to capacity)
static sem_t full_slots;  // Semaphore to track available items in buffer
                          // (initialized to 0)
static int producers_done =
    0; // Counter to track how many producers have finished

/*
Producer/consumer with semaphores (empty/full) and mutex on a size-5 buffer;
3 producers ×10 items, 2 consumers;
last producer inserts sentinels so consumers exit.
*/

// Helper to add item to circular buffer (assumes lock held)
static void enqueue(int item) {
  buffer[tail] = item;         // Place item at tail
  tail = (tail + 1) % BUF_CAP; // Advance tail cyclically
}

// Helper to remove item from circular buffer (assumes lock held)
static int dequeue(void) {
  int item = buffer[head];     // Retrieve item from head
  head = (head + 1) % BUF_CAP; // Advance head cyclically
  return item;
}

/* Producer pushes ITEMS_PER_PRODUCER items, then the final producer injects
 * sentinels */
static void *producer(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
    sem_wait(
        &empty_slots); // Wait for an empty slot (decrements count, blocks if 0)
    mutex_lock(&buf_lock); // Acquire exclusive access to the buffer
    enqueue(id * 100 + i); // Add the item to the buffer
    printf(1, "Producer %d: produced item %d\n", id, i);
    mutex_unlock(&buf_lock); // Release exclusive access
    sem_post(
        &full_slots); // Signal that a new item is available (increments count)
  }

  // Critical section to check if all producers are done
  mutex_lock(&buf_lock);
  producers_done++; // Increment count of finished producers
  if (producers_done == NUM_PRODUCERS) {
    // This is the last producer to finish, so it's responsible for stopping
    // consumers. We push one sentinel (-1) per consumer to signal them to exit.
    for (int i = 0; i < NUM_CONSUMERS; i++) {
      sem_wait(&empty_slots); // Ensure there is space for the sentinel
      enqueue(SENTINEL);      // Insert the sentinel value
      sem_post(&full_slots);  // Signal that "data" (the sentinel) is available
    }
  }
  mutex_unlock(&buf_lock);
  return 0;
}

static void *consumer(void *arg) {
  int id = (int)arg;
  while (1) {
    sem_wait(&full_slots);   // Wait for an item to be available (blocks if 0)
    mutex_lock(&buf_lock);   // Acquire lock to safely access buffer
    int item = dequeue();    // Remove item from buffer
    mutex_unlock(&buf_lock); // Release lock
    sem_post(&empty_slots);  // Signal that a slot is now empty

    if (item == SENTINEL) { // Check if we retrieved the stop signal
      // Sentinel consumed; signal received to exit. Put the sentinel back to
      // ensure it propagates to other consumers (broadcast style shutdown),
      // then terminate.

      sem_wait(&empty_slots);  // Wait for space to put the sentinel back
      mutex_lock(&buf_lock);   // Lock buffer
      enqueue(SENTINEL);       // Put sentinel back
      mutex_unlock(&buf_lock); // Unlock
      sem_post(&full_slots);   // Signal full

      printf(1, "Consumer %d: exiting (seen sentinel)\n", id);
      break; // Exit the loop
    }

    printf(1, "Consumer %d: consumed item %d\n", id, item); // Process the item
  }
  return 0;
}

int main(void) {
  thread_init();         // Initialize the user-level threading library
  mutex_init(&buf_lock); // Initialize the buffer mutex
  sem_init(&empty_slots,
           BUF_CAP);        // Initialize empty_slots to buffer capacity (5)
  sem_init(&full_slots, 0); // Initialize full_slots to 0 (buffer empty)

  int producers[NUM_PRODUCERS];
  int consumers[NUM_CONSUMERS];

  // Create producer threads
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    producers[i] = thread_create(producer, (void *)i);
  }
  // Create consumer threads
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    consumers[i] = thread_create(consumer, (void *)i);
  }

  // Wait for all producers to finish
  for (int i = 0; i < NUM_PRODUCERS; i++) {
    thread_join(producers[i]);
  }
  // Wait for all consumers to finish
  for (int i = 0; i < NUM_CONSUMERS; i++) {
    thread_join(consumers[i]);
  }

  printf(1, "pc_sem_test: PASS (all items processed)\n");
  exit();
}
