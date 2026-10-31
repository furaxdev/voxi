/*
 * The first program Voxi can run that isn't compiled into the kernel
 * itself: a standalone, statically-linked ELF64 binary with its own
 * entry point, loaded by kernel/elf.c into its own private page
 * tables (see kernel/mm/vmm.c) and run at ring 3. No libc, no crt0 --
 * just two syscalls: SYS_WRITE (1, fd/buf/len like real write()) and
 * SYS_EXIT (2, with an explicit exit code of 0 -- otherwise whatever
 * rdx last held gets misread as a "nonzero exit code" by anyone
 * waiting on this process).
 */
.intel_syntax noprefix

.section .text
.global _start
_start:
    mov edi, 1              /* fd 1 = stdout */
    lea rsi, [rip + msg1]
    mov edx, msg1_end - msg1
    mov eax, 1               /* SYS_WRITE */
    int 0x80

    mov edi, 1
    lea rsi, [rip + msg2]
    mov edx, msg2_end - msg2
    mov eax, 1
    int 0x80

    mov rdi, 0          /* exit code 0 */
    mov rax, 2          /* SYS_EXIT */
    int 0x80

hang:
    jmp hang            /* unreachable: sys_exit() never returns */

.section .rodata
msg1:
    .ascii "[hello] I am a separate ELF64 binary, loaded from ramfs,\n"
msg1_end:
msg2:
    .ascii "[hello] running in my own page tables with my own physical memory.\n"
msg2_end:
