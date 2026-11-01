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

/* The start of an actual libc (ulib.c): a bump+freelist malloc over a
 * static arena baked into every program's own .bss (no syscall needed
 * -- BSS is just zeroed pages the ELF loader already hands out), a
 * minimal printf (%d %u %x %c %s %%, no width/precision), and the
 * string.h basics. Still not a real libc -- no stdio buffering, no
 * locale, no float formatting -- but real enough that programs stop
 * hand-rolling integer-to-string conversions. */
void *umalloc(long size);
void ufree(void *ptr);
void uprintf(const char *fmt, ...);

int ustrcmp(const char *a, const char *b);
char *ustrcpy(char *dst, const char *src);
void *umemcpy(void *dst, const void *src, long n);
void *umemset(void *dst, int val, long n);

#endif
