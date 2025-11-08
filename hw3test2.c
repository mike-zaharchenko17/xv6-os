// assert clamps

#include "types.h"
#include "stat.h"
#include "user.h"
#include "testhelpers.h"

int main(void) {
    int my_pid = getpid();
    // assert is default
    int old = nice(my_pid, -5);
    // assert first nice call clamped to 0
    int prev = nice(my_pid, 99);
    // assert previous nice call clamped to 4
    int now = nice(my_pid, 4);

    printf(1, "old=%d prev=%d final=%d\n", old, prev, now);

    if (old != 2) {
        printf(1, "hw3test2: clamp FAIL; expected 2\n");
    }

    if (prev != 0) {
        printf(1, "hw3test2: clamp FAIL; expected 0\n");
    }

    if (now != 4) {
        printf(1, "hw3test2: clamp FAIL; expected 4\n");
    }

    printf(1, "hw3test1 (nice): OK\n");

    exit();
}