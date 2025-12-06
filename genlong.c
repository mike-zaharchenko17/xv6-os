#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    char long_line[1001];

    for (int i = 0; i < 1000; i++) {
        long_line[i] = 'A';
    }
    long_line[1000] = 0;

    printf(1, "%s\n", long_line);
    printf(1, "Done\n");

    exit();
}