#include <voxi/multiboot2.h>

static uint64_t boot_addr = 0;

void mb2_set_boot_addr(uint64_t addr) { boot_addr = addr; }
uint64_t mb2_get_boot_addr(void) { return boot_addr; }

struct mb2_tag *mb2_find_tag(uint64_t addr, uint32_t type) {
    uint32_t total_size = *(uint32_t *)(uintptr_t)addr;
    uint8_t *ptr = (uint8_t *)(uintptr_t)addr + 8; /* skip total_size + reserved */
    uint8_t *end = (uint8_t *)(uintptr_t)addr + total_size;

    while (ptr < end) {
        struct mb2_tag *tag = (struct mb2_tag *)ptr;
        if (tag->type == MB2_TAG_END) break;
        if (tag->type == type) return tag;
        /* tags are 8-byte aligned */
        ptr += (tag->size + 7) & ~7u;
    }
    return NULL;
}
