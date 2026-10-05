#include "idt.h"
#include <voxi/io.h>
#include "../../lib/kprintf.h"
#include "pic.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtp;
static interrupt_handler_t handlers[256];

extern void idt_load(struct idt_ptr *);

#define DECL_ISR(n) extern void isr##n(void);
#define DECL_IRQ(n) extern void irq##n(void);
DECL_ISR(0) DECL_ISR(1) DECL_ISR(2) DECL_ISR(3) DECL_ISR(4) DECL_ISR(5)
DECL_ISR(6) DECL_ISR(7) DECL_ISR(8) DECL_ISR(9) DECL_ISR(10) DECL_ISR(11)
DECL_ISR(12) DECL_ISR(13) DECL_ISR(14) DECL_ISR(15) DECL_ISR(16) DECL_ISR(17)
DECL_ISR(18) DECL_ISR(19) DECL_ISR(20) DECL_ISR(21) DECL_ISR(22) DECL_ISR(23)
DECL_ISR(24) DECL_ISR(25) DECL_ISR(26) DECL_ISR(27) DECL_ISR(28) DECL_ISR(29)
DECL_ISR(30) DECL_ISR(31)
DECL_IRQ(0) DECL_IRQ(1) DECL_IRQ(2) DECL_IRQ(3) DECL_IRQ(4) DECL_IRQ(5)
DECL_IRQ(6) DECL_IRQ(7) DECL_IRQ(8) DECL_IRQ(9) DECL_IRQ(10) DECL_IRQ(11)
DECL_IRQ(12) DECL_IRQ(13) DECL_IRQ(14) DECL_IRQ(15)

static void idt_set_gate(uint8_t n, void (*handler)(void)) {
    uint64_t addr = (uint64_t)handler;
    idt[n].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[n].selector    = 0x08; /* kernel code segment from boot GDT */
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E; /* present, ring0, 32-bit interrupt gate */
    idt[n].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

static const char *exception_name(uint8_t n) {
    static const char *names[32] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Overrun", "Invalid TSS", "Segment Not Present",
        "Stack Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
    };
    return n < 32 ? names[n] : "Unknown";
}

void register_interrupt_handler(uint8_t n, interrupt_handler_t handler) {
    handlers[n] = handler;
}

void isr_handler(registers_t *regs) {
    uint8_t n = (uint8_t)regs->int_no;

    if (n >= 32 && n < 48) {
        if (handlers[n]) handlers[n](regs);
        pic_send_eoi((uint8_t)(n - 32));
        return;
    }

    if (handlers[n]) {
        handlers[n](regs);
        return;
    }

    kprintf("\n!!! CPU EXCEPTION #%d: %s (err=%lx) !!!\n", (int)n, exception_name(n), regs->err_code);
    kprintf("rip=%p cs=%lx rflags=%lx\n", (void *)regs->rip, regs->cs, regs->rflags);
    kprintf("System halted.\n");
    for (;;) { __asm__ volatile ("cli; hlt"); }
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) idt_set_gate((uint8_t)i, 0);

    idt_set_gate(0, isr0);   idt_set_gate(1, isr1);   idt_set_gate(2, isr2);
    idt_set_gate(3, isr3);   idt_set_gate(4, isr4);   idt_set_gate(5, isr5);
    idt_set_gate(6, isr6);   idt_set_gate(7, isr7);   idt_set_gate(8, isr8);
    idt_set_gate(9, isr9);   idt_set_gate(10, isr10); idt_set_gate(11, isr11);
    idt_set_gate(12, isr12); idt_set_gate(13, isr13); idt_set_gate(14, isr14);
    idt_set_gate(15, isr15); idt_set_gate(16, isr16); idt_set_gate(17, isr17);
    idt_set_gate(18, isr18); idt_set_gate(19, isr19); idt_set_gate(20, isr20);
    idt_set_gate(21, isr21); idt_set_gate(22, isr22); idt_set_gate(23, isr23);
    idt_set_gate(24, isr24); idt_set_gate(25, isr25); idt_set_gate(26, isr26);
    idt_set_gate(27, isr27); idt_set_gate(28, isr28); idt_set_gate(29, isr29);
    idt_set_gate(30, isr30); idt_set_gate(31, isr31);

    idt_set_gate(32, irq0);  idt_set_gate(33, irq1);  idt_set_gate(34, irq2);
    idt_set_gate(35, irq3);  idt_set_gate(36, irq4);  idt_set_gate(37, irq5);
    idt_set_gate(38, irq6);  idt_set_gate(39, irq7);  idt_set_gate(40, irq8);
    idt_set_gate(41, irq9);  idt_set_gate(42, irq10); idt_set_gate(43, irq11);
    idt_set_gate(44, irq12); idt_set_gate(45, irq13); idt_set_gate(46, irq14);
    idt_set_gate(47, irq15);

    idt_load(&idtp);
}
