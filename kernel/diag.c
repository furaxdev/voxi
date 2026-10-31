#include "diag.h"
#include <voxi/io.h>
#include <voxi/multiboot2.h>
#include "lib/kprintf.h"
#include "lib/tsc.h"
#include "scheduler.h"
#include "syscall.h"
#include "heartbeat.h"
#include "boottime.h"
#include "mm/heap.h"
#include "generated_sloc.h"

extern void enter_usermode(void (*entry)(void), uint64_t user_stack_top) __attribute__((noreturn));

extern uint8_t _image_start[], kernel_end[];
extern uint8_t _text_start[], _text_end[];
extern uint8_t _rodata_start[], _rodata_end[];
extern uint8_t _data_start[], _data_end[];
extern uint8_t _bss_start[], _bss_end[];

void diag_memmap(void) {
    uint64_t addr = mb2_get_boot_addr();
    struct mb2_tag_mmap *mmap = (struct mb2_tag_mmap *)mb2_find_tag(addr, MB2_TAG_MMAP);
    if (!mmap) {
        kprintf("no memory map tag from the bootloader\n");
        return;
    }
    kprintf("addr                len (KiB)    type\n");
    uint8_t *p = (uint8_t *)mmap->entries;
    uint8_t *end = (uint8_t *)mmap + mmap->size;
    while (p < end) {
        struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
        const char *type = e->type == MB2_MEMORY_AVAILABLE ? "available" : "reserved";
        kprintf("%016lx    %-10lu   %s\n", (unsigned long)e->addr,
                (unsigned long)(e->len / 1024), type);
        p += mmap->entry_size;
    }
}

void diag_peek(uint64_t addr) {
    uint8_t v = *(volatile uint8_t *)addr;
    kprintf("[%016lx] = 0x%02x\n", (unsigned long)addr, v);
}

void diag_poke(uint64_t addr, uint8_t val) {
    *(volatile uint8_t *)addr = val;
    kprintf("[%016lx] <- 0x%02x\n", (unsigned long)addr, val);
}

void diag_hexdump(uint64_t addr, uint64_t len) {
    if (len > 512) len = 512;
    volatile uint8_t *p = (volatile uint8_t *)addr;
    for (uint64_t off = 0; off < len; off += 16) {
        kprintf("%016lx  ", (unsigned long)(addr + off));
        for (uint64_t i = 0; i < 16; i++) {
            if (off + i < len) kprintf("%02x ", p[off + i]);
            else kprintf("   ");
        }
        kprintf(" ");
        for (uint64_t i = 0; i < 16 && off + i < len; i++) {
            uint8_t c = p[off + i];
            kprintf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        kprintf("\n");
    }
}

void diag_inport(uint16_t port) {
    kprintf("in 0x%x = 0x%02x\n", port, inb(port));
}

void diag_outport(uint16_t port, uint8_t val) {
    outb(port, val);
    kprintf("out 0x%x <- 0x%02x\n", port, val);
}

void diag_cpuid(void) {
    uint32_t eax, ebx, ecx, edx;

    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    char vendor[13];
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = '\0';
    kprintf("vendor: %s  (max basic leaf: %lu)\n", vendor, (unsigned long)eax);

    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    kprintf("leaf 1: eax=%08lx ecx=%08lx edx=%08lx\n",
            (unsigned long)eax, (unsigned long)ecx, (unsigned long)edx);
    kprintf("  FPU=%d  PAE=%d  APIC=%d  SSE=%d  SSE2=%d  (edx feature bits)\n",
            (edx >> 0) & 1, (edx >> 6) & 1, (edx >> 9) & 1, (edx >> 25) & 1, (edx >> 26) & 1);
    kprintf("  SSE3=%d  SSSE3=%d  SSE4.1=%d  SSE4.2=%d  HYPERVISOR=%d  (ecx feature bits)\n",
            (ecx >> 0) & 1, (ecx >> 9) & 1, (ecx >> 19) & 1, (ecx >> 20) & 1, (ecx >> 31) & 1);
}

void diag_size(void) {
    kprintf("section   bytes\n");
    kprintf("text      %lu\n", (unsigned long)(_text_end - _text_start));
    kprintf("rodata    %lu\n", (unsigned long)(_rodata_end - _rodata_start));
    kprintf("data      %lu\n", (unsigned long)(_data_end - _data_start));
    kprintf("bss       %lu\n", (unsigned long)(_bss_end - _bss_start));
    kprintf("total image on disk: %lu bytes (%lu KiB)\n",
            (unsigned long)(_bss_start - _image_start),
            (unsigned long)((_bss_start - _image_start) / 1024));
    kprintf("total incl. bss (RAM footprint): %lu bytes (%lu KiB)\n",
            (unsigned long)(kernel_end - _image_start),
            (unsigned long)((kernel_end - _image_start) / 1024));
}

void diag_sloc(void) {
    kprintf("Voxi is %d lines of source code, total -- boot assembly, every\n", VOXI_SLOC);
    kprintf("driver, the scheduler, the whole thing. You could read all of it\n");
    kprintf("in an evening. Nobody reads all of Linux in a lifetime.\n");
}

#define BENCH_ITERATIONS 20000
#define BENCH_STACK_SIZE 8192

extern void switch_to(uint64_t *old_rsp_store, uint64_t new_rsp);

static uint8_t bench_stack[BENCH_STACK_SIZE];
static volatile uint64_t bench_remaining;
static uint64_t bench_task_rsp;
static uint64_t bench_caller_rsp;

static void bench_trampoline(void) {
    while (bench_remaining > 0) {
        bench_remaining--;
        switch_to(&bench_task_rsp, bench_caller_rsp);
    }
    for (;;) switch_to(&bench_task_rsp, bench_caller_rsp); /* never resumed again */
}

/* Measures the raw cost of switch_to() with an isolated, private pair of
 * stacks -- deliberately NOT going through the scheduler's ready queue.
 * The heartbeat task sits in that queue too and never yields on its
 * own, so a naive benchmark built on ordinary yield() calls can land on
 * it and then wait up to a full timer period (10ms) to get back off --
 * fine for a real workload, disastrous for measuring one switch's cost.
 * Interrupts are held off for the duration so a timer tick can't fire
 * mid-benchmark and have the scheduler mistake this private stack for
 * the shell task's own (it has no idea this is happening). */
void diag_switchbench(void) {
    uint64_t *sp = (uint64_t *)(bench_stack + BENCH_STACK_SIZE);
    *(--sp) = (uint64_t)bench_trampoline;
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; /* rbp, rbx, r12 */
    *(--sp) = 0; *(--sp) = 0; *(--sp) = 0; /* r13, r14, r15 */
    bench_task_rsp = (uint64_t)sp;
    bench_remaining = BENCH_ITERATIONS;

    __asm__ volatile ("cli" ::: "memory");
    uint64_t start = tsc_read();
    for (uint64_t i = 0; i < BENCH_ITERATIONS; i++) {
        switch_to(&bench_caller_rsp, bench_task_rsp);
    }
    uint64_t total = tsc_read() - start;
    __asm__ volatile ("sti" ::: "memory");

    kprintf("%lu round trips (2 switch_to calls each): %lu cycles total, %lu cycles/round trip\n",
            (unsigned long)BENCH_ITERATIONS, (unsigned long)total,
            (unsigned long)(total / BENCH_ITERATIONS));
}

void diag_syscallbench(void) {
    uint64_t start = tsc_read();
    for (uint64_t i = 0; i < BENCH_ITERATIONS; i++) {
        __asm__ volatile (
            "mov $0, %%rax\n"
            "int $0x80\n"
            ::: "rax"
        );
    }
    uint64_t total = tsc_read() - start;

    kprintf("%lu syscalls (SYS_NOP via int 0x80): %lu cycles total, %lu cycles/call\n",
            (unsigned long)BENCH_ITERATIONS, (unsigned long)total,
            (unsigned long)(total / BENCH_ITERATIONS));
}

void diag_freeze(void) {
    uint64_t cpms = boottime_cycles_per_ms();
    if (cpms == 0) cpms = 1000000;

    /* First, measure the heartbeat task's normal rate with interrupts
     * on, so we have something honest to compare the frozen window
     * against (an exact zero delta isn't achievable: the instant `sti`
     * re-enables interrupts, one already-pending tick can still slip
     * through before we get to read the counter again). */
    uint64_t r0 = heartbeat_count();
    uint64_t t0 = tsc_read();
    while (tsc_read() - t0 < cpms * 20);
    uint64_t normal_rate_per_ms = (heartbeat_count() - r0) / 20;

    kprintf("normal heartbeat rate: ~%lu increments/ms\n", (unsigned long)normal_rate_per_ms);
    kprintf("disabling interrupts for ~300ms, right after this line prints...\n");

    uint64_t before = heartbeat_count();
    uint64_t deadline = tsc_read() + cpms * 300;

    /* "memory" clobbers force GCC to treat these as full compiler
     * barriers, so the heartbeat_count() reads can't get reordered
     * across them. */
    __asm__ volatile ("cli" ::: "memory");
    while (tsc_read() < deadline);
    __asm__ volatile ("sti" ::: "memory");

    uint64_t actual = heartbeat_count() - before;
    uint64_t expected_if_unfrozen = normal_rate_per_ms * 300;

    kprintf("heartbeat advanced by %lu during the freeze\n", (unsigned long)actual);
    kprintf("it would normally advance by ~%lu in 300ms with interrupts on\n",
            (unsigned long)expected_if_unfrozen);
    kprintf("the CPU ran completely undisturbed, except for one tick that was\n");
    kprintf("already pending the instant interrupts came back on -- physically\n");
    kprintf("unavoidable, and still the only thing that got through.\n");
}

static void crashtest_ring3(void) {
    sys_write("[crashtest] about to divide by zero in ring3...\n");
    /* A plain C "1 / zero" is UB that GCC is free to (and does, at -O2)
     * optimize into a branch that never actually divides. Inline asm
     * guarantees the real idiv executes. */
    __asm__ volatile (
        "xorl %%edx, %%edx\n"
        "movl $1, %%eax\n"
        "xorl %%ecx, %%ecx\n"
        "divl %%ecx\n"
        ::: "eax", "ecx", "edx"
    );
    sys_write("[crashtest] you should never see this\n");
    sys_exit();
}

static void crashtest_task(void *arg) {
    (void)arg;
    uint8_t *user_stack = kmalloc(8192);
    uint64_t stack_top = (uint64_t)(user_stack + 8192);
    enter_usermode(crashtest_ring3, stack_top);
}

void diag_crashtest(void) {
    scheduler_spawn(crashtest_task, NULL);
    kprintf("spawned a ring3 task that will deliberately divide by zero.\n");
    kprintf("watch: the kernel survives. check 'lastfault' and 'killcount' after.\n");
}
