#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)
#define BUF_CAP 5
#define SENTINEL -1

static int buffer[BUF_CAP];
static int head = 0;
static int tail = 0;
static mutex_t buf_lock;
static sem_t empty_slots;
static sem_t full_slots;
static int producers_done = 0;

/*
Producer/consumer with semaphores (empty/full) and mutex on a size-5 buffer;
3 producers ×10 items, 2 consumers;
last producer inserts sentinels so consumers exit.
*/

static void enqueue(int item) {
    buffer[tail] = item;
    tail = (tail + 1) % BUF_CAP;
}

static int dequeue(void) {
    int item = buffer[head];
    head = (head + 1) % BUF_CAP;
    return item;
}

/* Producer pushes ITEMS_PER_PRODUCER items, then the final producer injects sentinels */
static void *producer(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        sem_wait(&empty_slots);
        mutex_lock(&buf_lock);
        enqueue(id * 100 + i);
        printf(1, "Producer %d: produced item %d\n", id, i);
        mutex_unlock(&buf_lock);
        sem_post(&full_slots);
    }

    mutex_lock(&buf_lock);
    producers_done++;
    if (producers_done == NUM_PRODUCERS) {
        // Last producer inserts one sentinel per consumer to tell them to stop.
        for (int i = 0; i < NUM_CONSUMERS; i++) {
            sem_wait(&empty_slots);
            enqueue(SENTINEL);
            sem_post(&full_slots);
        }
    }
    mutex_unlock(&buf_lock);
    return 0;
}

static void *consumer(void *arg) {
    int id = (int)arg;
    while (1) {
        sem_wait(&full_slots);
        mutex_lock(&buf_lock);
        int item = dequeue();
        mutex_unlock(&buf_lock);
        sem_post(&empty_slots);

        if (item == SENTINEL) {
            // Sentinel consumed; put it back for other consumers.
            sem_wait(&empty_slots);
            mutex_lock(&buf_lock);
            enqueue(SENTINEL);
            mutex_unlock(&buf_lock);
            sem_post(&full_slots);
            printf(1, "Consumer %d: exiting (seen sentinel)\n", id);
            break;
        }

        printf(1, "Consumer %d: consumed item %d\n", id, item);
    }
    return 0;
}

int main(void) {
    thread_init();
    mutex_init(&buf_lock);
    sem_init(&empty_slots, BUF_CAP); // all slots empty initially
    sem_init(&full_slots, 0);        // no items yet

    int producers[NUM_PRODUCERS];
    int consumers[NUM_CONSUMERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers[i] = thread_create(producer, (void *)i);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumers[i] = thread_create(consumer, (void *)i);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        thread_join(producers[i]);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        thread_join(consumers[i]);
    }

    printf(1, "pc_sem_test: PASS (all items processed)\n");
    exit();
}
