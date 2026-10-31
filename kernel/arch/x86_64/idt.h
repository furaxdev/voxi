#ifndef VOXI_IDT_H
#define VOXI_IDT_H

#include <voxi/types.h>

typedef struct {
    uint64_t rax, rbx, rcx, rdx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags;
} __attribute__((packed)) registers_t;

typedef void (*interrupt_handler_t)(registers_t *regs);

/* For a trap taken from ring3 (a privilege-level change), the CPU
 * pushes two more fields right after rflags: the user RSP and SS at
 * the time of the trap. registers_t doesn't name them -- they're only
 * meaningful for ring3-originated traps, never for ring0 ones -- but
 * sizeof(registers_t) lands exactly where they start, so code that
 * KNOWS it's handling a ring3 trap (fork/exec in syscall.c) can read
 * or rewrite them this way. Rewriting rip/tail.rsp before returning
 * from a syscall is exactly how exec() replaces "return to the old
 * program" with "start the new one". */
typedef struct {
    uint64_t rsp;
    uint64_t ss;
} registers_tail_t;

static inline registers_tail_t *registers_tail(registers_t *regs) {
    return (registers_tail_t *)((uint8_t *)regs + sizeof(registers_t));
}

void idt_init(void);
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);

/* Prints every populated IDT entry: the whole interrupt dispatch table
 * of the system, in full, on one screen. */
void idt_dump(void);

#endif
