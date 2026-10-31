#ifndef VOXI_SYSCALL_H
#define VOXI_SYSCALL_H

#include <voxi/types.h>

#define SYS_NOP    0
#define SYS_WRITE  1
#define SYS_EXIT   2
#define SYS_FORK   3  /* -> child pid in parent, 0 in child, -1 on failure */
#define SYS_EXEC   4  /* rdi = ramfs filename; doesn't return on success */
#define SYS_WAIT   5  /* blocks for any child, -> its exit code */
#define SYS_GETPID 6
#define SYS_READ   7  /* rdi = buf, rsi = maxlen; blocking line read from the keyboard */

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
