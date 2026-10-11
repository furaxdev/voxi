#ifndef VOXI_SYSCALL_H
#define VOXI_SYSCALL_H

#include <voxi/types.h>

#define SYS_WRITE 1
#define SYS_EXIT  2

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
