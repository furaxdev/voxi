#include "lastfault.h"
#include "lib/kprintf.h"

static bool have_fault = false;
static int last_vector = 0;
static uint64_t last_rip = 0;
static uint64_t last_cs = 0;
static int last_task_id = -1;

static const char *exception_name(int n) {
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
    return (n >= 0 && n < 32) ? names[n] : "Unknown";
}

void lastfault_record(int vector, uint64_t rip, uint64_t cs, int task_id) {
    have_fault = true;
    last_vector = vector;
    last_rip = rip;
    last_cs = cs;
    last_task_id = task_id;
}

void lastfault_print(void) {
    if (!have_fault) {
        kprintf("No task has crashed yet.\n");
        return;
    }
    kprintf("Last crash: task #%d, exception #%d (%s)\n",
            last_task_id, last_vector, exception_name(last_vector));
    kprintf("  rip=%p cs=0x%lx\n", (void *)last_rip, (unsigned long)last_cs);
    kprintf("  The kernel killed that one task and kept running everything else.\n");
}
