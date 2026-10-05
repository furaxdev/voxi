#ifndef VOXI_MULTIBOOT2_H
#define VOXI_MULTIBOOT2_H

#include <voxi/types.h>

#define MB2_TAG_END         0
#define MB2_TAG_CMDLINE     1
#define MB2_TAG_BOOTLOADER  2
#define MB2_TAG_MODULE      3
#define MB2_TAG_BASIC_MEM   4
#define MB2_TAG_BOOTDEV     5
#define MB2_TAG_MMAP        6
#define MB2_TAG_FRAMEBUFFER 8

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_basic_mem {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type; /* 1 = available RAM */
    uint32_t reserved;
};

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct mb2_mmap_entry entries[];
};

#define MB2_MEMORY_AVAILABLE 1

/* Finds the first tag of `type` in the multiboot2 info struct at `addr`.
 * Returns NULL if not found. */
struct mb2_tag *mb2_find_tag(uint64_t addr, uint32_t type);

#endif
