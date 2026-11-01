#include "ramfs.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"

extern const uint8_t hello_elf_start[], hello_elf_end[];
extern const uint8_t forkdemo_elf_start[], forkdemo_elf_end[];
extern const uint8_t pipedemo_elf_start[], pipedemo_elf_end[];
extern const uint8_t spin_elf_start[], spin_elf_end[];
extern const uint8_t libctest_elf_start[], libctest_elf_end[];
extern const uint8_t sigdemo_elf_start[], sigdemo_elf_end[];

typedef struct {
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
    const char *blurb;
} ramfs_entry_t;

static const ramfs_entry_t entries[] = {
    { "hello",    hello_elf_start,    hello_elf_end,    "userland/hello.S -- a standalone ELF64 binary" },
    { "forkdemo", forkdemo_elf_start, forkdemo_elf_end, "userland/fork_demo.c -- fork()+exec()+wait()" },
    { "pipedemo", pipedemo_elf_start, pipedemo_elf_end, "userland/pipe_demo.c -- pipe()+fork(), real IPC" },
    { "spin",     spin_elf_start,     spin_elf_end,     "userland/spin.c -- loops forever; try 'kill'" },
    { "libctest", libctest_elf_start, libctest_elf_end, "userland/libc_test.c -- umalloc()+uprintf()" },
    { "sigdemo",  sigdemo_elf_start,  sigdemo_elf_end,  "userland/sig_demo.c -- ignores SIGTERM; try 'kill'" },
};
#define RAMFS_COUNT (sizeof(entries) / sizeof(entries[0]))

bool ramfs_find(const char *name, const uint8_t **data, size_t *size) {
    for (size_t i = 0; i < RAMFS_COUNT; i++) {
        if (strcmp(name, entries[i].name) == 0) {
            *data = entries[i].start;
            *size = (size_t)(entries[i].end - entries[i].start);
            return true;
        }
    }
    return false;
}

void ramfs_list(void) {
    for (size_t i = 0; i < RAMFS_COUNT; i++) {
        kprintf("%-10s %5lu bytes  %s\n", entries[i].name,
                (unsigned long)(entries[i].end - entries[i].start), entries[i].blurb);
    }
    kprintf("all baked into the kernel image at build time (see 'diskls' for the\n");
    kprintf("real on-disk filesystem instead). run any of them with 'run NAME'.\n");
}
