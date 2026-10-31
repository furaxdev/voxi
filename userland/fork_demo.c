/*
 * The first C program to run on Voxi: compiled freestanding against
 * nothing but kernel/syscall.c's seven syscalls (see syscalls.S),
 * proving fork()/exec()/wait() actually work, not just exist.
 *
 * No libc: no malloc, no real strlen, nothing the kernel doesn't
 * provide directly. That's the honest state of things right now.
 */

typedef long pid_t;

extern void sys_write(const char *s);
extern void sys_exit_(long code);
extern pid_t sys_fork(void);
extern long sys_exec(const char *name);
extern long sys_wait(void);
extern pid_t sys_getpid(void);

int main(void) {
    sys_write("[fork_demo] pid ");
    /* no printf yet -- just prove we got a number back */
    pid_t me = sys_getpid();
    char buf[4] = {0};
    buf[0] = (char)('0' + (me / 10) % 10);
    buf[1] = (char)('0' + me % 10);
    buf[2] = '\n';
    sys_write(buf[0] == '0' ? &buf[1] : &buf[0]);

    sys_write("[fork_demo] about to fork()...\n");
    pid_t pid = sys_fork();

    if (pid == 0) {
        sys_write("[fork_demo] I am the child. Replacing myself with 'hello' via exec()...\n");
        sys_exec("hello");
        sys_write("[fork_demo] exec() failed -- this should not print\n");
        sys_exit_(1);
    } else {
        sys_write("[fork_demo] I am the parent. Waiting for my child to finish...\n");
        long code = sys_wait();
        sys_write(code == 0 ? "[fork_demo] child exited cleanly. fork/exec/wait all work.\n"
                             : "[fork_demo] child exited with a nonzero code.\n");
    }

    return 0;
}
