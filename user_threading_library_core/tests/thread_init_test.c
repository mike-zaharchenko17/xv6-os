#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

int main(int argc, char *argv[]) {
    thread_init();
    printf(1, "thread_init ran\n");
    exit();
}