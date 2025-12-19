#include "types.h"
#include "user.h"
#include "uthreads.h"

static mutex_t m;
static sem_t full;
static sem_t empty;

static int buf[5];

static int head = 0;
static int tail = 0;

static int total_produced = 0;
static int total_consumed = 0;

// producer acquires empty semaphore and proceeds if not negative
// producer waits if empty semaphore is neg (i.e., there are no empty slots)
// producer writes to the buffer at the tail
// producer notifies full semaphore when it produces an item

static void *producer(void *arg) {
    int producer_num = (int) arg;

    for (int i = 0; i < 10; i++) {
        // check should proceed
        sem_wait(&empty);

        // when proceed
        mutex_lock(&m);

        buf[(tail++ % 5)] = i; // mod tail by 5 (wraparound) and then increment it
        total_produced++;
        printf(1, "Producer %d: produced item %d\n", producer_num, i);

        mutex_unlock(&m);

        // notify full buffer and increment it
        sem_post(&full);
    }

    return 0;
}

// consumer reads from the buffer at the head

static void *consumer(void *arg) {
    int consumed = 0;
    int consumer_num = (int) arg;

    while (consumed != -1) {
        sem_wait(&full);

        mutex_lock(&m);

        consumed = buf[(head++ % 5)]; // ditto head
        total_consumed++;
        printf(1, "Consumer %d: consumed item %d\n", consumer_num, consumed);
        
        mutex_unlock(&m);

        sem_post(&empty);
    }

    return 0;
}

int main(void) {
    thread_init();

    mutex_init(&m);
    sem_init(&empty, 5);
    sem_init(&full, 0);

    int p1 = thread_create(producer, (void *) 1);
    int p2 = thread_create(producer, (void *) 2);
    int p3 = thread_create(producer, (void *) 3);

    int c1 = thread_create(consumer, (void *) 1);
    int c2 = thread_create(consumer, (void *) 2);

    thread_join(p1);
    thread_join(p2);
    thread_join(p3);

    // inject a poison pill per consumer

    sem_wait(&empty);

    mutex_lock(&m);
    buf[(tail++ % 5)] = -1;
    mutex_unlock(&m);

    sem_post(&full);
    
    sem_wait(&empty);

    mutex_lock(&m);    
    buf[(tail++ % 5)] = -1;
    mutex_unlock(&m);

    sem_post(&full);

    thread_join(c1);
    thread_join(c2);

    printf(1, "total items produced: %d\n", total_produced);
    // subtract two poison pills from the total consumed
    printf(1, "total items consumed: %d\n", (total_consumed-2));

    exit();
}