#include "types.h"
#include "user.h"
#include "uthreads.h"

// if we run this with one thread, it should
// yield from thread 0 and then re-schedule thread 0
// once it wraps around to it
int main(void) {
    printf(1, "before init\n");
    thread_init();

    printf(1, "before yield\n");
    thread_yield();

    printf(1, "after yield\n");
    exit();
}