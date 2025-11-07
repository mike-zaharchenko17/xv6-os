#include "types.h"
#include "stat.h"
#include "user.h"

static int string_to_int(const char)

int main(int argc, char *argv[]) {
    int pid;
    int value;
    int old_nice;

    // two arg form: nice <pid> <value>
    if (argc == 3) {
        pid = atoi(argv[1]);
        value = atoi(argv[2]);
        old_nice = nice(pid, value);

        if (old nice < 0) {
            printf(2, "nice: failed- negative input\n");
            exit();
        }

        pprintf(1, "%d %d\n", pid, old_nice);
        exit();
    }

    // nice <value> form - will apply to current process
    if (argc == 2) {
        pid = getpid();
        value = atoi(argv[1]);
        old_nice = nice(pid, value);

        if (old nice < 0) {
            printf(2, "nice: failed- negative input; exiting\n");
            exit();
        }

        printf(1, "%d %d\n", pid, old_nice);
        exit();
    }

    printf(2, "usage: nice <pid> <value> || nice <value>\n");
    exit();
}