/*
 * The first program Voxi can run that isn't compiled into the kernel
 * itself: a standalone, statically-linked ELF64 binary with its own
 * entry point, loaded by kernel/elf.c into its own private page
 * tables (see kernel/mm/vmm.c) and run at ring 3. No libc, no crt0 --
 * just two syscalls: SYS_WRITE (1) and SYS_EXIT (2, with an explicit
 * exit code of 0 in rdi -- otherwise whatever rdi last held, the msg2
 * pointer, gets misread as a "nonzero exit code" by anyone waiting on
 * this process).
 */
.intel_syntax noprefix

.section .text
.global _start
_start:
    lea rdi, [rip + msg1]
    mov rax, 1          /* SYS_WRITE */
    int 0x80

    lea rdi, [rip + msg2]
    mov rax, 1
    int 0x80

    mov rdi, 0          /* exit code 0 */
    mov rax, 2          /* SYS_EXIT */
    int 0x80

hang:
    jmp hang            /* unreachable: sys_exit() never returns */

.section .rodata
msg1:
    .asciz "[hello] I am a separate ELF64 binary, loaded from ramfs,\n"
msg2:
    .asciz "[hello] running in my own page tables with my own physical memory.\n"
