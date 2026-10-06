.intel_syntax noprefix

/* void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);
 * Saves the callee-saved registers of the current task onto its own
 * stack, stores the resulting rsp through old_rsp_store, then loads
 * new_rsp and restores the callee-saved registers found there before
 * returning -- into whatever context that stack belongs to. */
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
    ret
