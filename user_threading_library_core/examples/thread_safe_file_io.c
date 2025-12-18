#include "types.h" 
#include "user.h" 
#include "uthreads.h"

#define NPROD 3 
#define NCONS 2 
#define PER_PROD 10

// where fd[0] is the read end
// and fd[1] is the write end
static int fds[2];

// producer state
static mutex_t write_lock;

// consumer state
static mutex_t read_lock;
static mutex_t count_lock;
static int total_consumed = 0;

struct msg {
    int prod_id;
    int item;
}

static int write_n(int fd, const void *buf, int n) {
    int off = 0;
    const char *p = (const char *)buf;

    while (off < n) {
        int bytes_written = write(fd, p + off, n - off);
        if (bytes_written <= 0) {
            return -1;
        }
        off += bytes_written;
    }

    return 0;
}

static int read_n(int fd, const void *buf, int n) {
    int off = 0;

    char *p = (char *)buf;

    while (off < n) {
        int bytes_read = read(fd, p + off, n - off);
        if (bytes_read < 0) {
            // failure
            return -1;
        } 
        if (bytes_read == 0) {
            // EOF
            return 0;
        }
        off += bytes_read
    }
}

void consumer_process(void) {
    // consumer does not write, so we close write end
    close(fds[1]);

    // per process thread table init since threads are 1:N
    thread_init();

    // same with synch primitive init
    mutex_init(&read_lock);
    mutex_init(&count_lock);

    total_consumed = 0;

    int tids[NCONS];

    // create a thread for each of our producers
    for (int i = 0; i < NCONS; i++) {
        tids[i] = thread_create(consumer_thread, (void*)(i+1));
        if (tids[i] < 0) {
            printf(1, "consumer: thread_create failed\n");
            exit();
        }
    }

    for (int i = 0; i < NCONS; i++) {
        thread_join(tids[i]);
    }

    printf(1, "All items processed. Consumers exiting. total=%d (expected %d)\n", total_consumed, NPROD * PER_PROD);

    if (total_consumed != NPROD * PER_PROD) {
        printf(1, "[FAIL] lost items!\n");
        exit();
    }
}

// void *producer_thread(void* arg) {
//     int pid = (int) arg;

//     for (int i = 0; i < PER_PROD; i++) {
//         struct msg m;
//         m.prod_id = pid;
//         m.item = 1;

//         mutex_lock(&write_lock);

//         // write exactly one msg to the write end
//         if (write_n(fds[1], &m, sizeof(m)) < 0) {
//             printf(1, "producer %d: write failed\n", pid);
//             mutex_unlock(&write_lock);
//             exit();
//         }

//         printf(1, "Producer %d: produced item %d\n", pid, i);

//         mutex_unlock(&write_lock);

//         thread_yield();
//     }

//     return 0;
// }

void *consumer_thread(void *arg) {
    int cid = (int) arg;

    for (;;) {
        struct msg m;

        mutex_lock(&read_lock);
        int r = read_n(fds[0], &m, sizeof(m));
        mutex_unlock(&read_lock);

        if (r == 0) {
            return 0;
        }

        if (r < 0) {
            printf(1, "Consumer %d: read failed\n", cid);
            exit();
        }

        printf(1, "Consumer %d: consumed item %d (from Producer %d)\n", cid, m.item, m.prod_id);

        mutex_lock(&count_lock);
        total_consumed++;
        mutex_unlock(&count_lock);

        thread_yield();
    }

}

void producer_process(void) {
    // producer does not read, so we can close read end
    close(fds[0]);

    // per process thread table init since threads are 1:N
    thread_init();

    mutex_init(&write_lock);

    int tids[NPROD];

    // create a thread for each of our producers
    for (int i = 0; i < NPROD; i++) {
        tids[i] = thread_create(producer_thread, (void*)(i+1));
        if (tids[i] < 0) {
            printf(1, "producer: thread_create failed\n");
            exit();
        }
    }

    for (int i = 0; i < NPROD; i++) {
        thread_join(tids[i]);
    }

    close(fds[1]);
    exit();
}

void *producer_thread(void* arg) {
    int pid = (int) arg;

    for (int i = 0; i < PER_PROD; i++) {
        struct msg m;
        m.prod_id = pid;
        m.item = 1;

        mutex_lock(&write_lock);

        // write exactly one msg to the write end
        if (write_n(fds[1], &m, sizeof(m)) < 0) {
            printf(1, "producer %d: write failed\n", pid);
            mutex_unlock(&write_lock);
            exit();
        }

        printf(1, "Producer %d: produced item %d\n", pid, i);

        mutex_unlock(&write_lock);

        thread_yield();
    }

    return 0;
}

int main(void) {
    if (pipe(fds) < 0) {
        printf(1, "pipe failed\n"); 
        exit();
    }

    int pid = fork();
    if (pid < 0) {
        printf(1, "fork failed\n");
        exit();
    }

    if (pid == 0) {
        // child: producer proc
        producer_process();
    } else {
        // parent: consumer proc
        consumer_process();
        // reap producer
        wait();
        exit();
    }

    exit();
}