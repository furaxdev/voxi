#include "shell.h"
#include <voxi/io.h>
#include "lib/kprintf.h"
#include "lib/string.h"
#include "drivers/keyboard.h"
#include "drivers/vga.h"
#include "drivers/pit.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "heartbeat.h"

#define LINE_MAX 128

static void read_line(char *buf, size_t max) {
    size_t len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            buf[len] = '\0';
            kprintf("\n");
            return;
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                kprintf("\b");
            }
        } else if (len + 1 < max) {
            buf[len++] = c;
            kprintf("%c", c);
        }
    }
}

static void cmd_help(void) {
    kprintf("Voxi shell - available commands:\n");
    kprintf("  help      show this message\n");
    kprintf("  clear     clear the screen\n");
    kprintf("  echo TXT  print TXT\n");
    kprintf("  meminfo   show physical memory and heap stats\n");
    kprintf("  uptime    show ticks since boot\n");
    kprintf("  about     show kernel info\n");
    kprintf("  heartbeat show the background preemption counter (proves preemptive scheduling)\n");
    kprintf("  reboot    reboot the machine\n");
    kprintf("  halt      stop the CPU\n");
}

static void cmd_meminfo(void) {
    uint64_t total = pmm_total_frames();
    uint64_t free = pmm_free_frames();
    uint64_t used = total - free;
    kprintf("Physical memory:\n");
    kprintf("  total: %lu KiB (%lu frames)\n", (total * PAGE_SIZE) / 1024, total);
    kprintf("  used:  %lu KiB (%lu frames)\n", (used * PAGE_SIZE) / 1024, used);
    kprintf("  free:  %lu KiB (%lu frames)\n", (free * PAGE_SIZE) / 1024, free);
    kprintf("Kernel heap:\n");
    kprintf("  total: %lu KiB\n", (unsigned long)(heap_total_bytes() / 1024));
    kprintf("  used:  %lu KiB\n", (unsigned long)(heap_used_bytes() / 1024));
}

static void cmd_uptime(void) {
    uint64_t ticks = pit_ticks();
    kprintf("ticks: %lu  (~%lu.%lu s)\n", ticks, ticks / PIT_HZ, (ticks % PIT_HZ) * 10 / PIT_HZ);
}

static void cmd_about(void) {
    kprintf("Voxi -- a kernel built from scratch by Furax (FuraxDev)\n");
    kprintf("Architecture: x86_64, booted via Multiboot2/GRUB\n");
}

static void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile ("hlt");
}

void shell_run(void) {
    char line[LINE_MAX];

    vga_set_color(VGA_LGREEN, VGA_BLACK);
    kprintf("\nWelcome to Voxi! Type 'help' for a list of commands.\n");
    vga_set_color(VGA_LGRAY, VGA_BLACK);

    for (;;) {
        vga_set_color(VGA_LCYAN, VGA_BLACK);
        kprintf("voxi> ");
        vga_set_color(VGA_LGRAY, VGA_BLACK);

        read_line(line, LINE_MAX);

        if (line[0] == '\0') {
            continue;
        } else if (strcmp(line, "help") == 0) {
            cmd_help();
        } else if (strcmp(line, "clear") == 0) {
            vga_clear();
        } else if (strncmp(line, "echo ", 5) == 0) {
            kprintf("%s\n", line + 5);
        } else if (strcmp(line, "meminfo") == 0) {
            cmd_meminfo();
        } else if (strcmp(line, "uptime") == 0) {
            cmd_uptime();
        } else if (strcmp(line, "about") == 0) {
            cmd_about();
        } else if (strcmp(line, "heartbeat") == 0) {
            kprintf("heartbeat counter: %lu (climbing in the background while you type)\n",
                    (unsigned long)heartbeat_count());
        } else if (strcmp(line, "reboot") == 0) {
            kprintf("Rebooting...\n");
            reboot();
        } else if (strcmp(line, "halt") == 0) {
            kprintf("System halted.\n");
            __asm__ volatile ("cli");
            for (;;) __asm__ volatile ("hlt");
        } else {
            kprintf("unknown command: %s (try 'help')\n", line);
        }
    }
}
