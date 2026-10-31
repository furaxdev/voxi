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

/* Identical to enter_usermode, except it zeroes rax first. iretq never
 * touches general-purpose registers, so a fresh task resuming straight
 * into ring3 (never having "returned from a syscall" in the normal
 * sense) keeps whatever garbage was last in the physical register --
 * fork()'s child specifically needs rax=0 there, since that's the
 * only way C code sees "fork() returned 0" distinguishing it from the
 * parent (who gets the child's pid via the normal syscall-return path,
 * where the kernel explicitly sets regs->rax before returning). */
.global enter_usermode_child
enter_usermode_child:
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x1B
    push rsi
    push 0x202
    push 0x23
    push rdi

    xor rax, rax
    iretq
