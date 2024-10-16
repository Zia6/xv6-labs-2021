#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAX_LINE 1024

int readline(int fd, char *buf, int max_len) {
    int n = 0;
    char c;
    while (n < max_len - 1) {
        if (read(fd, &c, 1) <= 0) {
            break;
        }
        if (c == '\n') {
            break;
        }
        buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "Usage: xargs <command> [args...]\n");
        exit(1);
    }

    char line[MAX_LINE];
    char *new_argv[MAXARG];
    int i;

    for (i = 1; i < argc && i < MAXARG - 1; i++) {
        new_argv[i - 1] = argv[i];
    }
    int base_argc = i - 1;
    while (readline(0, line, MAX_LINE) > 0) {
        if (base_argc + 1 >= MAXARG) {
            fprintf(2, "Too many arguments\n");
            exit(1);
        }
        new_argv[base_argc] = line;
        new_argv[base_argc + 1] = 0;
        if (fork() == 0) {
            exec(new_argv[0], new_argv);
            fprintf(2, "exec %s failed\n", new_argv[0]);
            exit(1);
        } else {
            wait(0);
        }
    }

    exit(0);
}