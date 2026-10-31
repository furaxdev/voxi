#ifndef VOXI_SYSCALL_H
#define VOXI_SYSCALL_H

#include <voxi/types.h>

#define SYS_NOP    0
#define SYS_WRITE  1  /* rdi = fd, rsi = buf, rdx = len */
#define SYS_EXIT   2
#define SYS_FORK   3  /* -> child pid in parent, 0 in child, -1 on failure */
#define SYS_EXEC   4  /* rdi = ramfs filename; doesn't return on success */
#define SYS_WAIT   5  /* blocks for any child, -> its exit code */
#define SYS_GETPID 6
#define SYS_READ   7  /* rdi = fd, rsi = buf, rdx = maxlen */
#define SYS_PIPE   8  /* rdi = int[2] out: [0]=read fd, [1]=write fd */
#define SYS_CLOSE  9  /* rdi = fd */
#define SYS_KILL   10 /* rdi = pid, rsi = signal number */

#define SIGKILL 9  /* the only signal actually delivered right now */
#define SIGTERM 15 /* accepted but currently handled exactly like SIGKILL */

#define FD_STDIN  0
#define FD_STDOUT 1
#define FD_STDERR 2

void syscall_init(void);

/* Ring3-callable wrappers (plain functions, but only valid to call from
 * code actually running at CPL3 via `int 0x80`). */
void sys_write(const char *s);
void sys_exit(void) __attribute__((noreturn));

/* Entry point for a ring3 demo task: drops to CPL3 via enter_usermode()
 * and never returns to its caller (sys_exit() ends it through the
 * scheduler instead). */
void usermode_demo_entry(void *arg);

#endif
