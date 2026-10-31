#ifndef VOXI_ULIB_H
#define VOXI_ULIB_H

/*
 * Not a libc. The entire "standard library" every userland program on
 * Voxi has right now: raw syscall wrappers (syscalls.S) plus the two
 * tiny helpers below, because even strlen() and puts() don't exist
 * yet. This is exactly the gap the README talks about when it says a
 * real libc port is still ahead.
 */

typedef long pid_t;

extern long sys_write(int fd, const char *buf, long len);
extern void sys_exit_(long code);
extern pid_t sys_fork(void);
extern long sys_exec(const char *name);
extern long sys_wait(void);
extern pid_t sys_getpid(void);
extern long sys_read(int fd, char *buf, long maxlen);
extern long sys_pipe(int *fds);
extern long sys_close(int fd);
extern long sys_kill(long pid, long sig);

static inline long ustrlen(const char *s) {
    long n = 0;
    while (s[n]) n++;
    return n;
}

static inline void uputs(int fd, const char *s) {
    sys_write(fd, s, ustrlen(s));
}

#endif
