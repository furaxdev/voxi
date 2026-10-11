.intel_syntax noprefix

/* void enter_usermode(void (*entry)(void), uint64_t user_stack_top) -- never returns.
 * rdi = entry point, rsi = top of the stack to give the ring3 code.
 * Builds a 5-item iretq frame by hand and drops to CPL3. There is no
 * real memory isolation yet (same page tables, just marked
 * user-accessible), but CS/SS really are ring3 -- privileged
 * instructions genuinely fault, as the demo deliberately shows. */
.global enter_usermode
enter_usermode:
    mov ax, 0x1B        /* user data selector | RPL3 */
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x1B            /* ss */
    push rsi              /* rsp */
    push 0x202            /* rflags: IF=1 */
    push 0x23              /* cs: user code | RPL3 */
    push rdi                /* rip: entry */
    iretq
