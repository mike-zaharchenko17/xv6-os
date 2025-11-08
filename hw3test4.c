// sleeper wakes repeatedly; ensure it runs again

#include "types.h"
#include "stat.h"
#include "user.h"

int main(void) {
    int p = fork();
    if (p == 0) {
        for (int i = 0; i < 10; i++) {
            printf(1, "S");
            sleep(5);
        }
        printf(1, "\nchild done\n");
        exit();
    }

    // Parent busy-waits to let child sleep/wake many times
    uint start = uptime();
    while ((int)(uptime() - start) < 120) {}

    wait();

    printf(1, "hw3test4 (sleepwake): OK\n");

    exit();
}