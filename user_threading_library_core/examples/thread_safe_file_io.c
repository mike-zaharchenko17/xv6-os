#include "types.h" 
#include "user.h" 
#include "uthreads.h"

static int fds[2];

int main() {
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
        // child: consumer proc
    } else {
        // parent: producer proc
    }

    exit();
}