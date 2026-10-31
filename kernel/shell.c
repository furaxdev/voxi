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
#include "boottime.h"
#include "scheduler.h"
#include "lastfault.h"
#include "diag.h"
#include "arch/x86_64/idt.h"
#include "arch/x86_64/gdt.h"
#include "lib/tsc.h"
#include "generated_sloc.h"
#include "drivers/rtc.h"
#include "process.h"
#include "fs/ramfs.h"

#define LINE_MAX 128

static void execute_line(char *line);

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
    kprintf("  help               show this message\n");
    kprintf("  clear              clear the screen\n");
    kprintf("  echo TXT           print TXT\n");
    kprintf("  meminfo            exact physical memory + heap stats\n");
    kprintf("  memmap             full bootloader memory map\n");
    kprintf("  uptime             PIT ticks since boot\n");
    kprintf("  about              kernel info\n");
    kprintf("  boottime           TSC-measured time to reach this shell\n");
    kprintf("  freq               measured CPU clock speed\n");
    kprintf("  cycles             raw TSC value, live\n");
    kprintf("  clock              PIT ticks and TSC cycles side by side\n");
    kprintf("  heartbeat          background preemption counter\n");
    kprintf("  tasks / ps         every task in the system and its state\n");
    kprintf("  killcount          tasks killed by hardware faults so far\n");
    kprintf("  lastfault          details of the last crash caught\n");
    kprintf("  crashtest          spawn a ring3 task that divides by zero on purpose\n");
    kprintf("  idt                dump the whole interrupt descriptor table\n");
    kprintf("  gdt                dump the whole GDT + TSS\n");
    kprintf("  cpuid              raw CPUID vendor + feature bits\n");
    kprintf("  size               kernel image size by section\n");
    kprintf("  sloc               total lines of source code in Voxi\n");
    kprintf("  switchbench        measure context-switch cost in CPU cycles\n");
    kprintf("  syscallbench       measure int 0x80 syscall cost in CPU cycles\n");
    kprintf("  freeze             cli for a moment, prove nothing else ran\n");
    kprintf("  peek ADDR          read one byte of memory (hex addr)\n");
    kprintf("  poke ADDR VAL      write one byte of memory (hex addr, hex val)\n");
    kprintf("  hexdump ADDR LEN   dump memory (hex addr, decimal/hex len)\n");
    kprintf("  inport PORT        read one byte from an I/O port (hex)\n");
    kprintf("  outport PORT VAL   write one byte to an I/O port (hex, hex)\n");
    kprintf("  lsmod              list loaded modules (there are none, ever)\n");
    kprintf("  uname              one-line system summary\n");
    kprintf("  t N | CMD          run CMD after N seconds, in the background\n");
    kprintf("  t -h HH:MM:SS | CMD  same, at that time of day (RTC-based)\n");
    kprintf("  ls                 list files in the ramfs\n");
    kprintf("  run NAME           load+run a ramfs ELF binary as a real isolated process\n");
    kprintf("  reboot             reboot the machine\n");
    kprintf("  halt               stop the CPU\n");
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
    kprintf("Booted to this shell in %lu ms (%lu CPU cycles) -- try 'boottime'\n",
            (unsigned long)boottime_ms(), (unsigned long)boottime_cycles());
}

static void cmd_boottime(void) {
    kprintf("Time from kernel_main() to this shell: %lu ms (%lu CPU cycles, TSC-measured)\n",
            (unsigned long)boottime_ms(), (unsigned long)boottime_cycles());
    kprintf("A stock Linux boot doesn't even finish decompressing vmlinuz in that time.\n");
}

static void cmd_freq(void) {
    uint64_t cpms = boottime_cycles_per_ms();
    kprintf("measured CPU clock: %lu cycles/ms  (~%lu MHz)\n",
            (unsigned long)cpms, (unsigned long)(cpms / 1000));
}

static void cmd_cycles(void) {
    kprintf("TSC: %lu\n", (unsigned long)tsc_read());
}

static void cmd_clock(void) {
    kprintf("PIT ticks: %lu    TSC cycles: %lu\n",
            (unsigned long)pit_ticks(), (unsigned long)tsc_read());
}

static void cmd_uname(void) {
    kprintf("Voxi x86_64 -- %d lines of source, %lu ms boot, no modules, no mercy\n",
            VOXI_SLOC, (unsigned long)boottime_ms());
}

typedef struct {
    uint64_t deadline_ticks;
    char command[LINE_MAX];
} t_job_t;

static void t_task_entry(void *arg) {
    t_job_t *job = (t_job_t *)arg;
    while (pit_ticks() < job->deadline_ticks) yield();
    kprintf("\n[t] running: %s\n", job->command);
    execute_line(job->command);
    kfree(job);
}

/* `t 5 | echo hi`        -- run `echo hi` 5 seconds from now
 * `t -h 14:56:00 | echo hi` -- run it at that time of day instead
 *
 * `args` is everything after "t " on the line, e.g. "5 | echo hi" or
 * "-h 14:56:00 | echo hi". Runs as a separate kernel task (see
 * t_task_entry above), so the shell's own input loop is never blocked
 * -- you get the prompt back immediately and can keep typing while it
 * waits, same as every other task in this system. */
static void cmd_t(char *args) {
    char *pipe_pos = strchr(args, '|');
    if (!pipe_pos) {
        kprintf("usage: t SECONDS | COMMAND   or   t -h HH:MM:SS | COMMAND\n");
        return;
    }
    *pipe_pos = '\0';
    char *command = pipe_pos + 1;
    while (*command == ' ' || *command == '\t') command++;

    char specbuf[64];
    strncpy(specbuf, args, sizeof(specbuf) - 1);
    specbuf[sizeof(specbuf) - 1] = '\0';
    char *sc = specbuf;
    char *tok = next_token(&sc);
    uint64_t seconds_to_wait;

    if (!tok) {
        kprintf("usage: t SECONDS | COMMAND   or   t -h HH:MM:SS | COMMAND\n");
        return;
    } else if (strcmp(tok, "-h") == 0) {
        char *timestr = next_token(&sc);
        if (!timestr) {
            kprintf("usage: t -h HH:MM:SS | COMMAND\n");
            return;
        }
        long th = (long)parse_dec(timestr);
        char *p1 = strchr(timestr, ':');
        long tm = p1 ? (long)parse_dec(p1 + 1) : 0;
        char *p2 = p1 ? strchr(p1 + 1, ':') : NULL;
        long ts = p2 ? (long)parse_dec(p2 + 1) : 0;

        uint8_t ch, cm, cs;
        rtc_read(&ch, &cm, &cs);
        long now_secs = (long)ch * 3600 + (long)cm * 60 + (long)cs;
        long target_secs = th * 3600 + tm * 60 + ts;
        long delta = target_secs - now_secs;
        if (delta <= 0) delta += 86400;
        seconds_to_wait = (uint64_t)delta;

        kprintf("current RTC time ~%02ld:%02ld:%02ld, running at %02ld:%02ld:%02ld (in %lu s)\n",
                (long)ch, (long)cm, (long)cs, th, tm, ts, (unsigned long)seconds_to_wait);
    } else {
        seconds_to_wait = parse_dec(tok);
        kprintf("running in %lu s...\n", (unsigned long)seconds_to_wait);
    }

    t_job_t *job = kmalloc(sizeof(t_job_t));
    if (!job) {
        kprintf("out of memory, can't schedule that\n");
        return;
    }
    job->deadline_ticks = pit_ticks() + seconds_to_wait * PIT_HZ;
    strncpy(job->command, command, LINE_MAX - 1);
    job->command[LINE_MAX - 1] = '\0';

    scheduler_spawn(t_task_entry, job);
    kprintf("[t] scheduled in the background -- the shell is free, keep typing.\n");
}

static void reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile ("hlt");
}

static void execute_line(char *line) {
        char *cursor = line;
        char *cmd = next_token(&cursor);

        if (!cmd) {
            return;
        } else if (strcmp(cmd, "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd, "clear") == 0) {
            vga_clear();
        } else if (strcmp(cmd, "echo") == 0) {
            kprintf("%s\n", cursor);
        } else if (strcmp(cmd, "meminfo") == 0) {
            cmd_meminfo();
        } else if (strcmp(cmd, "memmap") == 0) {
            diag_memmap();
        } else if (strcmp(cmd, "uptime") == 0) {
            cmd_uptime();
        } else if (strcmp(cmd, "about") == 0) {
            cmd_about();
        } else if (strcmp(cmd, "boottime") == 0) {
            cmd_boottime();
        } else if (strcmp(cmd, "freq") == 0) {
            cmd_freq();
        } else if (strcmp(cmd, "cycles") == 0) {
            cmd_cycles();
        } else if (strcmp(cmd, "clock") == 0) {
            cmd_clock();
        } else if (strcmp(cmd, "heartbeat") == 0) {
            kprintf("heartbeat counter: %lu (climbing in the background while you type)\n",
                    (unsigned long)heartbeat_count());
        } else if (strcmp(cmd, "tasks") == 0 || strcmp(cmd, "ps") == 0) {
            scheduler_dump();
        } else if (strcmp(cmd, "killcount") == 0) {
            kprintf("tasks killed by hardware faults: %lu\n",
                    (unsigned long)scheduler_kill_count());
            kprintf("tasks that exited cleanly (SYS_EXIT): %lu\n",
                    (unsigned long)scheduler_exit_count());
        } else if (strcmp(cmd, "lastfault") == 0) {
            lastfault_print();
        } else if (strcmp(cmd, "crashtest") == 0) {
            diag_crashtest();
        } else if (strcmp(cmd, "idt") == 0) {
            idt_dump();
        } else if (strcmp(cmd, "gdt") == 0) {
            gdt_dump();
        } else if (strcmp(cmd, "cpuid") == 0) {
            diag_cpuid();
        } else if (strcmp(cmd, "size") == 0) {
            diag_size();
        } else if (strcmp(cmd, "sloc") == 0) {
            diag_sloc();
        } else if (strcmp(cmd, "switchbench") == 0) {
            diag_switchbench();
        } else if (strcmp(cmd, "syscallbench") == 0) {
            diag_syscallbench();
        } else if (strcmp(cmd, "freeze") == 0) {
            diag_freeze();
        } else if (strcmp(cmd, "peek") == 0) {
            char *a = next_token(&cursor);
            if (a) diag_peek(parse_hex(a)); else kprintf("usage: peek ADDR\n");
        } else if (strcmp(cmd, "poke") == 0) {
            char *a = next_token(&cursor);
            char *v = next_token(&cursor);
            if (a && v) diag_poke(parse_hex(a), (uint8_t)parse_hex(v));
            else kprintf("usage: poke ADDR VAL\n");
        } else if (strcmp(cmd, "hexdump") == 0) {
            char *a = next_token(&cursor);
            char *l = next_token(&cursor);
            if (a && l) diag_hexdump(parse_hex(a), parse_hex(l));
            else kprintf("usage: hexdump ADDR LEN\n");
        } else if (strcmp(cmd, "inport") == 0) {
            char *p = next_token(&cursor);
            if (p) diag_inport((uint16_t)parse_hex(p)); else kprintf("usage: inport PORT\n");
        } else if (strcmp(cmd, "outport") == 0) {
            char *p = next_token(&cursor);
            char *v = next_token(&cursor);
            if (p && v) diag_outport((uint16_t)parse_hex(p), (uint8_t)parse_hex(v));
            else kprintf("usage: outport PORT VAL\n");
        } else if (strcmp(cmd, "lsmod") == 0) {
            kprintf("0 modules loaded. There is nothing in this kernel you can't see\n");
            kprintf("with 'size' and 'sloc' -- everything it can do is already compiled in.\n");
        } else if (strcmp(cmd, "uname") == 0) {
            cmd_uname();
        } else if (strcmp(cmd, "t") == 0) {
            cmd_t(cursor);
        } else if (strcmp(cmd, "ls") == 0) {
            ramfs_list();
        } else if (strcmp(cmd, "run") == 0) {
            char *name = next_token(&cursor);
            if (name) process_spawn(name); else kprintf("usage: run NAME\n");
        } else if (strcmp(cmd, "reboot") == 0) {
            kprintf("Rebooting...\n");
            reboot();
        } else if (strcmp(cmd, "halt") == 0) {
            kprintf("System halted.\n");
            __asm__ volatile ("cli");
            for (;;) __asm__ volatile ("hlt");
        } else {
            kprintf("unknown command: %s (try 'help')\n", cmd);
        }
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
        execute_line(line);
    }
}
