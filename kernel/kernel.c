#include <voxi/types.h>
#include <voxi/multiboot2.h>
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "drivers/pit.h"
#include "drivers/keyboard.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/pic.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/kprintf.h"
#include "scheduler.h"
#include "shell.h"

extern uint8_t kernel_end[];

static void demo_task(void *arg) {
    const char *name = (const char *)arg;
    for (int i = 1; i <= 3; i++) {
        kprintf("[task %s] iteration %d\n", name, i);
        yield();
    }
}

static void run_multitasking_demo(void) {
    scheduler_init();
    scheduler_spawn(demo_task, (void *)"A");
    scheduler_spawn(demo_task, (void *)"B");
    scheduler_spawn(demo_task, (void *)"C");

    kprintf("[sched] 3 cooperative tasks spawned, running them to completion...\n");
    while (!scheduler_all_done()) yield();
    kprintf("[sched] all demo tasks finished\n");
}

void kernel_main(uint64_t multiboot_addr) {
    vga_init();
    serial_init();

    vga_set_color(VGA_YELLOW, VGA_BLACK);
    kprintf("Voxi kernel starting...\n");
    vga_set_color(VGA_LGRAY, VGA_BLACK);

    idt_init();
    pic_remap();
    kprintf("[idt]  interrupt table loaded\n");

    pmm_init(multiboot_addr, (uint64_t)kernel_end);
    heap_init(1024); /* 4 MiB initial heap */

    pit_init(PIT_HZ);
    keyboard_init();

    __asm__ volatile ("sti");
    kprintf("[irq]  interrupts enabled (timer @ %dHz, keyboard)\n", PIT_HZ);

    run_multitasking_demo();

    shell_run();

    for (;;) __asm__ volatile ("hlt");
}
