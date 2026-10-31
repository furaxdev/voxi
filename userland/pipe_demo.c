/*
 * Real IPC: a pipe between a parent and its own forked child, same
 * mechanism a shell uses for `cmd1 | cmd2`. The child writes into the
 * write end and exits; the parent reads whatever made it through the
 * kernel's pipe_t ring buffer (kernel/syscall.c) out the read end.
 */
#include "ulib.h"

int main(void) {
    int fds[2];
    sys_pipe(fds); /* fds[0] = read end, fds[1] = write end */

    pid_t pid = sys_fork();

    if (pid == 0) {
        sys_close(fds[0]); /* the child only writes */
        const char *msg = "[pipe_demo] hello through a pipe, from the child!\n";
        sys_write(fds[1], msg, ustrlen(msg));
        sys_close(fds[1]);
        sys_exit_(0);
    }

    sys_close(fds[1]); /* the parent only reads */
    char buf[128] = {0};
    long n = sys_read(fds[0], buf, sizeof(buf) - 1);
    sys_close(fds[0]);

    uputs(1, "[pipe_demo] parent received ");
    char numbuf[4] = {0};
    numbuf[0] = (char)('0' + (n / 10) % 10);
    numbuf[1] = (char)('0' + n % 10);
    numbuf[2] = ' ';
    uputs(1, numbuf[0] == '0' ? &numbuf[1] : &numbuf[0]);
    uputs(1, "bytes: ");
    sys_write(1, buf, n);

    sys_wait();
    return 0;
}
