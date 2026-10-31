.intel_syntax noprefix

.section .text
.global _start
.extern main
_start:
    call main
    mov edi, eax      /* main's return value becomes the exit code */
    mov rax, 2          /* SYS_EXIT */
    int 0x80
hang:
    jmp hang            /* unreachable */
