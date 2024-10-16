#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int p1[2], p2[2];
    pipe(p1); // 创建管道1
    pipe(p2); // 创建管道2

    if (fork() == 0) {
        // 子进程
        char buf[5];
        read(p1[0], buf, 5); // 从管道1读取
        printf("%d: received ping\n", getpid());
        write(p2[1], "pong\n", 5); // 向管道2写入
    } else {
        // 父进程
        write(p1[1], "ping\n", 5); // 向管道1写入
        char buf[5];
        read(p2[0], buf, 5); // 从管道2读取
        printf("%d: received pong\n", getpid());
    }

    exit(0);
}
