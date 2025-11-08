// starve: verifies that low priority processes don't stall

#include "types.h"
#include "stat.h"
#include "user.h"
#include "testhelpers.h"

// declare prototype
int yield(void);

extern int nice(int pid, int val);

int main(void) {
    int hi = fork();

    if (hi == 0) {
        nice(getpid(), 0);

        for (int i = 0; i < 200; i++) {
            // be a little nice to the system; yield occasionally
            if ((i % 10) == 0) {
                yield();
            }
        }

        printf(1, "HI done\n");
        exit();
    }

    int n = 4;

    for (int i = 0; i < n; i++) {
        int p = fork();
        if (p == 0) {
            nice(getpid(), 4);
            // do some work; print at least once
            for (int j = 0; j < 1000000; j++) {
                if ((j % 200000) == 0) {
                    printf(1, "lo%d ping\n", getpid());
                }
            }
            printf(1, "lo%d done\n", getpid());
            exit();
        }
    }

    for (int i = 0; i < n + 1; i++) {
        wait();
    }

    printf(1, "hw3test5 (starve): OK (progress observed)\n");
    exit();
}