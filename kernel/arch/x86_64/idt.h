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

void idt_init(void);
void register_interrupt_handler(uint8_t n, interrupt_handler_t handler);

#endif
