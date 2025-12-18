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
    // per process thread table init since threads are 1:N
    thread_init();

    mutex_init(&read_lock);

    // same with synch primitive init

}

void *consumer_thread(void *arg) {

}

void producer_process(void) {
    // per process thread table init since threads are 1:N
    thread_init();

    mutex_init(&write_lock);
}

void *producer_thread(void* arg) {
    
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