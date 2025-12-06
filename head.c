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

char buf[512];

/*
design: 
the buffer keeps 'rolling' and reading 512 bytes
*/
void head_fd(int fd, int N) {
    int line_count = 0;
    int bytes_read;
    int done = 0;

    // read 512 bytes to the buffer
    while (!done && (bytes_read = read(fd, buf, sizeof(buf))) > 0) {
        // go byte by byte
        for (int b = 0; b < bytes_read; b++) {
            // print each byte (character) to stdout
            printf(1, "%c", buf[b]);
            // check if newline character
            if (buf[b] == '\n') {
                // increment line count
                line_count++;
                // if we've reached the max number of lines,
                // break out of both loops
                if (line_count == N) {
                    done = 1;
                    break;
                }
            }
        }
    }
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
            // handle -nN case
            if (all_digits(arg + 1)) {
                N = atoi(arg + 1);
                i++;
            // handle -n N case
            } else if (i + 1 < argc && all_digits(argv[i+1])) {
                N = atoi(argv[i+1]);
                i += 2;
            // fallthrough
            } else {
                printf(2, "head: invalid usage\n");
                exit();
            }
        // handle -N case
        } else if (all_digits(arg)) {
            N = atoi(arg);
            i++;
        } else {
            printf(2, "head: invalid usage\n");
            exit();
        }
    }

    // once we've parsed the argument, if our ptr is
    // gte the argc, no files have been provided so we
    // must read from stdin
    int num_files_remaining = argc - i;
    int idx_last_file = argc - 1;

    if (i >= argc) {
        head_fd(0, N);
    } else {
        // handle one or more files
        // the parser code above should put the pointer where the flag
        // arguments end, so this iterates through the rest of the parameters,
        // which, by convention, are files
        for (; i < argc; i++) {
            if (num_files_remaining > 1) {
                // decide whether we need to do headers
                printf(1, "==> %s <==\n", argv[i]);
            }
            int fd = open(argv[i], 0);
            if (fd < 0) {
                printf(2, "head: cannot open %s\n", argv[i]);
                continue;
            }
            head_fd(fd, N);
            close(fd);
            // decide whether we need to print a \n
            // will auto-fail if there is only one file
            // as i will == idx_last_file
            if (i != idx_last_file) printf(2, "\n");
        }
    }

    exit();
}