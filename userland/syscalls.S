.intel_syntax noprefix

/* Thin syscall wrappers, the only "libc" userland programs get right
 * now. Each just loads the syscall number and traps -- arguments are
 * already in the right registers (rdi, rsi, ...) per the System V ABI,
 * which happens to be exactly the calling convention int 0x80 expects
 * too, so there's nothing to shuffle. */

.macro SYSCALL name, num
.global \name
\name:
    mov rax, \num
    int 0x80
    ret
.endm

SYSCALL sys_write, 1
SYSCALL sys_exit_, 2
SYSCALL sys_fork, 3
SYSCALL sys_exec, 4
SYSCALL sys_wait, 5
SYSCALL sys_getpid, 6
SYSCALL sys_read, 7
SYSCALL sys_pipe, 8
SYSCALL sys_close, 9
SYSCALL sys_kill, 10
