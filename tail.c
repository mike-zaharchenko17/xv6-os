#include "types.h"
#include "stat.h"
#include "user.h"

char buf[512];

void tail(int fd, int N) {
    char *lines[N];
    int idx = 0, count = 0;
    char linebuf[512];
    int lp = 0;
    char c;

    // init buffer
    for (int i = 0; i < N; i++)
        lines[i] = 0;

    // Read byte‐by‐byte, assemble lines
    while (read(fd, &c, 1) == 1) {
        linebuf[lp++] = c;

        if (lp >= sizeof(linebuf)-1)
            lp = sizeof(linebuf)-2;

        if (c == '\n') {
            // terminate and store this line
            linebuf[lp] = '\0';
            // free old if present
            if (lines[idx]) {
                free(lines[idx]);
            }
            // strdup into our circular slot
            lines[idx] = malloc(lp+1);
            strcpy(lines[idx], linebuf);

            idx = (idx + 1) % N;
            count++;
            lp = 0;
        }
    }
    // handle final line if no trailing newline
    if (lp > 0) {
        linebuf[lp] = '\0';
        if (lines[idx]) free(lines[idx]);
        lines[idx] = malloc(lp+1);
        strcpy(lines[idx], linebuf);
        idx = (idx + 1) % N;
        count++;
    }

    // figure out where to start printing
    int toprint = count < N ? count : N;
    int start = count < N ? 0 : idx;

    // print in order
    for (int i = 0; i < toprint; i++) {
        char *ln = lines[(start + i) % N];
        if (ln) {
            write(1, ln, strlen(ln));
            free(ln);
        }
    }
}

int all_digits(const char* s) {
    if (s == 0 || *s == 0) return 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
    }
    return 1;
}

int parse_uint(const char* s) {
  int v = 0;
  for (const char *p = s; *p; p++) {
    v = v * 10 + (*p - '0');
  }
  return v;
}

int main(int argc, char *argv[]) {
    int fd;
    int i = 1;
    int N = 10;

    if (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        // expect -n N
        if (argv[i][1] == 'n' && argv[i][2] == 0) {
            if (i + 1 >= argc || !all_digits(argv[i + 1])) {
                printf(2, "tail: -n requires a number");
                exit();
            }
            N = parse_uint(argv[i + 1]);
            i += 2;
        } else {
            if (!all_digits(argv[i] + 1)) {
               printf(2, "tail: invalid option %s\n", argv[i]);
               exit();
            }
            N = parse_uint(argv[i] + 1);
            i += 1;
        }
    }

    if (i >= argc) {
        tail(0, N);
    } else {
        // iterate over the length of argc
        // start at wherever we left off because we're down to the operands 
        for (; i < argc; i++) {
            // open the file
            fd = open(argv[i], 0);

            // check for error
            if (fd < 0) {
                printf(1, "tail: cannot open %s\n", argv[i]);
                continue;
            }

            // pass the file descriptor to tail and perform operation
            tail(fd, N);

            // close out of the file
            close(fd);
        }
    }
    exit();
}