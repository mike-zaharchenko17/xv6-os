#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_READERS 3
#define NUM_WRITERS 2
#define READER_ITERS 5
#define WRITER_ITERS 3

// Structure to hold synchronization state for Reader-Writer Lock
typedef struct rw_state {
  mutex_t lock;        // Mutex to protect all state variables below
  cond_t readers_ok;   // Condition variable for waiting readers
  cond_t writers_ok;   // Condition variable for waiting writers
  int readers_active;  // Count of readers currently holding the lock
  int writers_waiting; // Count of writers waiting for the lock (key for
                       // priority)
  int writer_active;   // Flag: 1 if a writer holds the lock, 0 otherwise
} rw_state_t;

static rw_state_t rw;      // Shared reader-writer lock state
static int shared_val = 0; // Shared resource protected by the lock

static void reader_lock(rw_state_t *s) {
  mutex_lock(&s->lock); // Access shared state

  // Writers have priority. If a writer is writing (writer_active)
  // OR if any writer is waiting (writers_waiting > 0), this reader must wait.
  while (s->writer_active || s->writers_waiting > 0) {
    // Sleep until a writer signals that readers may proceed
    cond_wait(&s->readers_ok, &s->lock); // Wait on readers_ok condition
  }
  s->readers_active++;    // Registered as an active reader
  mutex_unlock(&s->lock); // Release state lock
}

static void reader_unlock(rw_state_t *s) {
  mutex_lock(&s->lock); // Access shared state
  s->readers_active--;  // Decrement active reader count

  // If this was the last reader, and there are writers waiting, wake one up.
  if (s->readers_active == 0 && s->writers_waiting > 0) {
    // Readers are gone; hand over to a writer
    cond_signal(&s->writers_ok); // Signal one writer to proceed
  }
  // Note: If no writers are waiting, we don't need to do anything special
  // (other readers are already concurrent, and new readers can enter if no
  // writers wait)

  mutex_unlock(&s->lock); // Release state lock
}

static void writer_lock(rw_state_t *s) {
  mutex_lock(&s->lock); // Access shared state
  s->writers_waiting++; // Register intention to write (establishes priority
                        // over new readers)

  // Wait until there are no active readers AND no active writers.
  while (s->writer_active || s->readers_active > 0) {
    // Sleep; woken either by writer_unlock or reader_unlock
    cond_wait(&s->writers_ok, &s->lock); // Wait on writers_ok condition
  }
  s->writers_waiting--;   // No longer waiting, now active
  s->writer_active = 1;   // Mark as active writer
  mutex_unlock(&s->lock); // Release state lock
}

static void writer_unlock(rw_state_t *s) {
  mutex_lock(&s->lock); // Access shared state
  s->writer_active = 0; // Mark writer as inactive

  // Priority policy: if other writers are waiting, wake them first (Writer
  // Priority).
  if (s->writers_waiting > 0) {
    // Let the next writer run; readers stay blocked
    cond_signal(&s->writers_ok); // Wake one wanting writer
  } else {
    // If no writers waiting, wake ALL waiting readers.
    cond_broadcast(&s->readers_ok); // Broadcast to all readers
  }
  mutex_unlock(&s->lock); // Release state lock
}

static void *reader(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < READER_ITERS; i++) {
    reader_lock(&rw);   // Acquire read lock (may wait for writers)
    int v = shared_val; // Critical section (read only)
    printf(1, "Reader %d: reading value = %d\n", id, v);
    reader_unlock(&rw); // Release read lock
    thread_yield();     // Yield to make scheduling interesting
  }
  return 0;
}

static void *writer(void *arg) {
  int id = (int)arg;
  for (int i = 0; i < WRITER_ITERS; i++) {
    writer_lock(&rw); // Acquire write lock (exclusive)
    shared_val++;     // Critical section (modify)
    printf(1, "Writer %d: wrote new value = %d\n", id, shared_val);
    writer_unlock(&rw); // Release write lock
    thread_yield();
  }
  return 0;
}

int main(void) {
  thread_init();
  mutex_init(&rw.lock);
  cond_init(&rw.readers_ok);
  cond_init(&rw.writers_ok);
  rw.readers_active = 0;
  rw.writers_waiting = 0;
  rw.writer_active = 0;

  int rthreads[NUM_READERS];
  int wthreads[NUM_WRITERS];

  // Create reader threads
  for (int i = 0; i < NUM_READERS; i++) {
    rthreads[i] = thread_create(reader, (void *)i);
  }
  // Create writer threads
  for (int i = 0; i < NUM_WRITERS; i++) {
    wthreads[i] = thread_create(writer, (void *)i);
  }

  // Join all
  for (int i = 0; i < NUM_READERS; i++) {
    thread_join(rthreads[i]);
  }
  for (int i = 0; i < NUM_WRITERS; i++) {
    thread_join(wthreads[i]);
  }

  printf(1, "rw_lock_test: PASS\n");
  exit();
}
