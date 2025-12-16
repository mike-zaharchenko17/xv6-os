#include "types.h"
#include "stat.h"
#include "user.h"
#include "uthreads.h"

static int tests_run = 0;
static int tests_failed = 0;
static int tests_passed = 0;

void run_ok(const char *name, void (*fn)(void)) {
    tests_run++;

    // create child
    int pid = fork();

    if (pid < 0) {
        printf(1, "[FAIL] %s (fork failed)", name);
        tests_failed++;
        return;
    }

    if (pid == 0) {
        fn();
        printf(1, "[PASS] %s\n", name);
        tests_passed++;
        exit();
    }

    // reap
    wait();
}

void run_expect_exit(const char *name, void (*fn)(void)) {
    tests_run++;

    int pid = fork();

    if (pid < 0) {
        printf(1, "[FAIL] %s (fork failed)", name);
        tests_failed++;
        return;
    }

    if (pid == 0) {
        fn();
        printf(1, "[FAIL] %s (expected exit, but returned)\n", name);
        tests_failed++;
        exit();
    }

    // child terminated either by exit() or by trap

    // since misuse paths call exit(), termination is the expected outcome.

    wait(); 

    tests_passed++;

    printf(1, "[PASS] %s (terminated as expected)\n", name);
}