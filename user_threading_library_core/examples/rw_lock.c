#include "types.h"
#include "user.h"
#include "uthreads.h"

#define NUM_READERS 3
#define NUM_WRITERS 2
#define READER_ITERS 5
#define WRITER_ITERS 3

typedef struct rw_state {
    mutex_t lock;
    cond_t readers_ok;
    cond_t writers_ok;
    int readers_active;
    int writers_waiting;
    int writer_active;
} rw_state_t;

static rw_state_t rw;
static int shared_val = 0;

static void reader_lock(rw_state_t *s) {
    mutex_lock(&s->lock);
    while (s->writer_active || s->writers_waiting > 0) {
        cond_wait(&s->readers_ok, &s->lock);
    }
    s->readers_active++;
    mutex_unlock(&s->lock);
}

static void reader_unlock(rw_state_t *s) {
    mutex_lock(&s->lock);
    s->readers_active--;
    // If this was the last reader, prefer waking a waiting writer.
    if (s->readers_active == 0 && s->writers_waiting > 0) {
        cond_signal(&s->writers_ok);
    }
    mutex_unlock(&s->lock);
}

static void writer_lock(rw_state_t *s) {
    mutex_lock(&s->lock);
    s->writers_waiting++;
    while (s->writer_active || s->readers_active > 0) {
        cond_wait(&s->writers_ok, &s->lock);
    }
    s->writers_waiting--;
    s->writer_active = 1;
    mutex_unlock(&s->lock);
}

static void writer_unlock(rw_state_t *s) {
    mutex_lock(&s->lock);
    s->writer_active = 0;
    if (s->writers_waiting > 0) {
        cond_signal(&s->writers_ok); // give priority to next writer
    } else {
        cond_broadcast(&s->readers_ok); // let readers proceed
    }
    mutex_unlock(&s->lock);
}

static void *reader(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < READER_ITERS; i++) {
        reader_lock(&rw);
        int v = shared_val;
        printf(1, "Reader %d: reading value = %d\n", id, v);
        reader_unlock(&rw);
        thread_yield();
    }
    return 0;
}

static void *writer(void *arg) {
    int id = (int)arg;
    for (int i = 0; i < WRITER_ITERS; i++) {
        writer_lock(&rw);
        shared_val++;
        printf(1, "Writer %d: wrote new value = %d\n", id, shared_val);
        writer_unlock(&rw);
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

    for (int i = 0; i < NUM_READERS; i++) {
        rthreads[i] = thread_create(reader, (void *)i);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        wthreads[i] = thread_create(writer, (void *)i);
    }

    for (int i = 0; i < NUM_READERS; i++) {
        thread_join(rthreads[i]);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        thread_join(wthreads[i]);
    }

    printf(1, "rw_lock_test: PASS\n");
    exit();
}
