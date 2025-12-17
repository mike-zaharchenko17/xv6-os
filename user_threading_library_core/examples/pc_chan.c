#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ITEMS_PER_PRODUCER 10
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

static channel_t *ch;
static int producers_done = 0;
static mutex_t done_lock;

/* Producers send ITEMS_PER_PRODUCER integers; final producer closes channel */
static void *producer(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        int val = id * 100 + i;
        channel_send(ch, (void *)val);
        printf(1, "Producer %d: produced item %d\n", id, i);
    }

    mutex_lock(&done_lock);
    producers_done++;
    if (producers_done == NUM_PRODUCERS) {
        channel_close(ch); // signal no more items
    }
    mutex_unlock(&done_lock);
    return 0;
}

static void *consumer(void *arg) {
    int id = (int)arg;
    int received = 0;
    while (1) {
        void *data = 0;
        int rc = channel_recv(ch, &data);
        if (rc < 0) {
            break; // closed and drained
        }
        received++;
        printf(1, "Consumer %d: consumed item %d\n", id, (int)data);
    }
    printf(1, "Consumer %d: exiting after %d items\n", id, received);
    return 0;
}

int main(void) {
    thread_init();
    mutex_init(&done_lock);
    ch = channel_create(5); // bounded buffer inside channel
    if (!ch) {
        printf(1, "pc_chan_test: failed to create channel\n");
        exit();
    }

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

    printf(1, "pc_chan_test: PASS (channel closed and drained)\n");
    exit();
}
