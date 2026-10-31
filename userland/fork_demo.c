/*
 * The first C program to run on Voxi: compiled freestanding against
 * nothing but kernel/syscall.c's syscalls (see syscalls.S and
 * ulib.h), proving fork()/exec()/wait() actually work, not just exist.
 */
#include "ulib.h"

int main(void) {
    uputs(1, "[fork_demo] pid ");
    /* no printf yet -- just prove we got a number back */
    pid_t me = sys_getpid();
    char buf[4] = {0};
    buf[0] = (char)('0' + (me / 10) % 10);
    buf[1] = (char)('0' + me % 10);
    buf[2] = '\n';
    uputs(1, buf[0] == '0' ? &buf[1] : &buf[0]);

    uputs(1, "[fork_demo] about to fork()...\n");
    pid_t pid = sys_fork();

    if (pid == 0) {
        uputs(1, "[fork_demo] I am the child. Replacing myself with 'hello' via exec()...\n");
        sys_exec("hello");
        uputs(1, "[fork_demo] exec() failed -- this should not print\n");
        sys_exit_(1);
    } else {
        uputs(1, "[fork_demo] I am the parent. Waiting for my child to finish...\n");
        long code = sys_wait();
        uputs(1, code == 0 ? "[fork_demo] child exited cleanly. fork/exec/wait all work.\n"
                            : "[fork_demo] child exited with a nonzero code.\n");
    }

    return 0;
}
