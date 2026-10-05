#include "pmm.h"
#include <voxi/multiboot2.h>
#include "../lib/string.h"
#include "../lib/kprintf.h"

/* Identity-mapped region set up at boot is 1 GiB; never hand out frames
 * beyond that until the VMM learns to map more. */
#define MAPPED_LIMIT (1ull * 1024 * 1024 * 1024)

static uint8_t *bitmap;
static uint64_t total_frames;
static uint64_t free_frames;
static uint64_t bitmap_bytes;

static inline void bitmap_set(uint64_t frame) {
    bitmap[frame / 8] |= (uint8_t)(1u << (frame % 8));
}
static inline void bitmap_clear(uint64_t frame) {
    bitmap[frame / 8] &= (uint8_t)~(1u << (frame % 8));
}
static inline bool bitmap_test(uint64_t frame) {
    return (bitmap[frame / 8] >> (frame % 8)) & 1;
}

static uint64_t align_up(uint64_t v, uint64_t a) {
    return (v + a - 1) & ~(a - 1);
}

void pmm_init(uint64_t multiboot_addr, uint64_t kernel_end) {
    uint64_t highest = 0;
    struct mb2_tag_mmap *mmap = (struct mb2_tag_mmap *)mb2_find_tag(multiboot_addr, MB2_TAG_MMAP);

    if (mmap) {
        uint8_t *p = (uint8_t *)mmap->entries;
        uint8_t *end = (uint8_t *)mmap + mmap->size;
        while (p < end) {
            struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
            uint64_t region_end = e->addr + e->len;
            if (e->type == MB2_MEMORY_AVAILABLE && region_end > highest)
                highest = region_end;
            p += mmap->entry_size;
        }
    }

    if (highest > MAPPED_LIMIT) highest = MAPPED_LIMIT;
    if (highest == 0) highest = MAPPED_LIMIT;

    total_frames = highest / PAGE_SIZE;
    bitmap_bytes = (total_frames + 7) / 8;

    bitmap = (uint8_t *)align_up(kernel_end, PAGE_SIZE);
    memset(bitmap, 0xFF, bitmap_bytes); /* default: everything reserved */
    free_frames = 0;

    if (mmap) {
        uint8_t *p = (uint8_t *)mmap->entries;
        uint8_t *end = (uint8_t *)mmap + mmap->size;
        while (p < end) {
            struct mb2_mmap_entry *e = (struct mb2_mmap_entry *)p;
            if (e->type == MB2_MEMORY_AVAILABLE) {
                uint64_t start_frame = e->addr / PAGE_SIZE;
                uint64_t frame_count = e->len / PAGE_SIZE;
                for (uint64_t f = start_frame; f < start_frame + frame_count && f < total_frames; f++) {
                    bitmap_clear(f);
                    free_frames++;
                }
            }
            p += mmap->entry_size;
        }
    }

    /* Reserve everything from physical 0 up to end of our own bitmap
     * (kernel image + bitmap storage), plus the low 1MiB legacy area. */
    uint64_t reserved_end = (uint64_t)bitmap + bitmap_bytes;
    uint64_t reserved_frames = align_up(reserved_end, PAGE_SIZE) / PAGE_SIZE;
    for (uint64_t f = 0; f < reserved_frames && f < total_frames; f++) {
        if (!bitmap_test(f)) free_frames--;
        bitmap_set(f);
    }

    kprintf("[pmm] %lu frames total, %lu free (%lu MiB)\n",
            total_frames, free_frames, (free_frames * PAGE_SIZE) / (1024 * 1024));
}

uint64_t pmm_alloc_frame(void) {
    for (uint64_t f = 0; f < total_frames; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            free_frames--;
            return f * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint64_t phys_addr) {
    uint64_t f = phys_addr / PAGE_SIZE;
    if (f >= total_frames) return;
    if (bitmap_test(f)) {
        bitmap_clear(f);
        free_frames++;
    }
}

uint64_t pmm_total_frames(void) { return total_frames; }
uint64_t pmm_free_frames(void) { return free_frames; }
