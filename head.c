#include "types.h"
#include "stat.h"
#include "user.h"

int all_digits(char *s) {
    if (*s == 0) {
        return 0;
    }

    for (; *s; s++) {
        if (*s < '0' || *s > '9') {
            return 0;
        }
    }

    return 1;
}

int main(int argc, char *argv[]) {
    /*
    head FILE
    head 
    head FILE1 FILE2 FILE3 .. 
    head -N
    head -N FILE
    head -N FILE1 FILE2 FILE3 ..
    head -n N 
    head -n N FILE
    head -n N FILE1 FILE2 FILE3 ..
    <commands> | head 
    */

    /*
    case 1: flag provided
        case 1.1: -N
            case 1.1.1: nothing after
                read from stdin
            case 1.1.2: single file
                read N lines from that file
            case 1.1.3: multiple files
                read N lines from those multiple files
        case 1.2: -n N
            case 1.2.1: nothing after
                read N lines from stdin
            case 1.2.2: single file
                read N lines from that one file
            case 1.2.3: multiple files
                read N lines from each of those files
    case 2: no flags
        case 2.1: nothing after
            read 10 (default) lines from stdin
        case 2.2: single file
            read 10 (default) lines from stdin
        case 2.3: multiple files
            read 10 (default) lines from each of those files
    */

    int N = 10; //default
    int i = 1;

    // if arguments are provided and there is a flag
    if (i < argc && argv[i][0] == '-') {
        // move arg pointer to one after -
        char *arg = argv[i] + 1;

        // handle -n case
        if (arg[0] == 'n') {
            printf(1, "-n flag detected\n");
            // handle -nN case
            if (all_digits(arg + 1)) {
                printf(1, "handling -nN case\n");
                N = atoi(arg + 1);
                printf(1, "head: N = %d\n", N);
                i++;
            // handle -n N case
            } else if (i + 1 < argc && all_digits(argv[i+1])) {
                printf(1, "handling -n N case\n");
                N = atoi(argv[i+1]);
                printf(1, "head: N = %d\n", N);
                i += 2;
            // fallthrough
            } else {
                printf(1, "head: invalid -n usage\n");
                exit();
            }
        // handle -N case
        } else if (all_digits(arg)) {
            printf(1, "handling -N case\n");
            N = atoi(arg);
            printf(1, "head: N = %d\n", N);
            i++;
        }
    }

    exit();
}