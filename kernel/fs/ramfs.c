#include "ramfs.h"
#include "../lib/string.h"
#include "../lib/kprintf.h"

extern const uint8_t hello_elf_start[], hello_elf_end[];

bool ramfs_find(const char *name, const uint8_t **data, size_t *size) {
    if (strcmp(name, "hello") == 0) {
        *data = hello_elf_start;
        *size = (size_t)(hello_elf_end - hello_elf_start);
        return true;
    }
    return false;
}

void ramfs_list(void) {
    kprintf("hello  %lu bytes  (userland/hello.S, a freestanding ELF64 binary\n",
            (unsigned long)(hello_elf_end - hello_elf_start));
    kprintf("       baked into the kernel image at build time -- run it with 'run hello')\n");
}
