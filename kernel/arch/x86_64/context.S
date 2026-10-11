.intel_syntax noprefix

/* void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);
 * Saves the callee-saved registers of the current task onto its own
 * stack, stores the resulting rsp through old_rsp_store, then loads
 * new_rsp and restores the callee-saved registers found there before
 * returning -- into whatever context that stack belongs to. Used for
 * every kernel-task switch (both voluntary yield() and timer-driven
 * preemption): a plain register save/restore + `ret`, deliberately NOT
 * an iretq -- a same-privilege iretq from a manually relocated stack
 * (not immediately following the hardware's own interrupt push) turned
 * out to raise a spurious #GP under QEMU/TCG. Ring3 entry/exit still
 * uses the hardware's own interrupt machinery (iretq in usermode.S,
 * and the untouched isr_common_stub epilogue), which are the
 * well-trodden paths and work fine. */
.global switch_to
switch_to:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15

    mov [rdi], rsp

    mov rsp, rsi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    /* Whoever we're resuming might be resuming mid-timer-interrupt (IF
     * was cleared on entry to that interrupt gate and won't be restored
     * until its own iretq, which may be a while from now), or might be a
     * brand new task that never had interrupts enabled at all. Either
     * way, every task that's actually running must have IF=1, or the
     * timer can never fire again to preempt it. */
    sti
    ret
