#include "ramfs.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"

extern const uint8_t hello_elf_start[], hello_elf_end[];
extern const uint8_t forkdemo_elf_start[], forkdemo_elf_end[];

bool ramfs_find(const char *name, const uint8_t **data, size_t *size) {
    if (strcmp(name, "hello") == 0) {
        *data = hello_elf_start;
        *size = (size_t)(hello_elf_end - hello_elf_start);
        return true;
    }
    if (strcmp(name, "forkdemo") == 0) {
        *data = forkdemo_elf_start;
        *size = (size_t)(forkdemo_elf_end - forkdemo_elf_start);
        return true;
    }
    return false;
}

void ramfs_list(void) {
    kprintf("hello     %lu bytes  (userland/hello.S -- run with 'run hello')\n",
            (unsigned long)(hello_elf_end - hello_elf_start));
    kprintf("forkdemo  %lu bytes  (userland/fork_demo.c -- fork()+exec()+wait(),\n",
            (unsigned long)(forkdemo_elf_end - forkdemo_elf_start));
    kprintf("          run with 'run forkdemo')\n");
    kprintf("both baked into the kernel image at build time -- no real disk yet.\n");
}
