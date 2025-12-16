#include "types.h"
#include "user.h"
#include "uthreads.h"

// run a test in a child process; expect success, exit if fail

void run_ok(const char *name, void (*fn)(void));

// run a test in a child process; expect controlled exit

void run_expect_exit(const char *name, void (*fn)(void));