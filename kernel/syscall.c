#include "syscall.h"
#include "arch/x86_64/idt.h"
#include "scheduler.h"
#include "mm/heap.h"
#include "lib/kprintf.h"

extern void enter_usermode(void (*entry)(void), uint64_t user_stack_top) __attribute__((noreturn));

static void syscall_handler(registers_t *regs) {
    switch (regs->rax) {
        case SYS_WRITE:
            kprintf("%s", (const char *)regs->rdi);
            break;
        case SYS_EXIT:
            kprintf("[kernel] task %d exited via syscall\n", scheduler_current()->id);
            scheduler_kill_current_and_switch(); /* never returns */
            break;
        default:
            kprintf("[kernel] unknown syscall %lu from task %d\n",
                    (unsigned long)regs->rax, scheduler_current()->id);
            break;
    }
}

void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
}

void sys_write(const char *s) {
    __asm__ volatile (
        "mov $1, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}

void sys_exit(void) {
    __asm__ volatile (
        "mov $2, %%rax\n"
        "int $0x80\n"
        ::: "rax"
    );
    for (;;); /* unreachable */
}

static void ring3_main(void) {
    sys_write("[ring3] Hello from user mode! Running at CPL=3.\n");
    sys_write("[ring3] Trying a privileged instruction (cli) -- the kernel should catch the fault...\n");
    __asm__ volatile ("cli");
    sys_write("[ring3] you should NEVER see this line\n");
    sys_exit();
}

void usermode_demo_entry(void *arg) {
    (void)arg;
    uint8_t *user_stack = kmalloc(8192);
    uint64_t stack_top = (uint64_t)(user_stack + 8192);
    enter_usermode(ring3_main, stack_top);
}
