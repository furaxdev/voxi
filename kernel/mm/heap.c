#include "heap.h"
#include "pmm.h"
#include "../lib/kprintf.h"

typedef struct block_header {
    size_t size; /* usable bytes following this header */
    bool free;
    struct block_header *prev;
    struct block_header *next;
} block_header_t;

static block_header_t *heap_head = NULL;
static block_header_t *heap_tail = NULL;
static uint64_t heap_next_phys = 0; /* next contiguous frame expected, for growth checks */
static size_t total_bytes = 0;

#define ALIGN8(x) (((x) + 7u) & ~7ull)
#define MIN_SPLIT 32

static bool heap_extend(size_t pages) {
    uint64_t first = 0;
    for (size_t i = 0; i < pages; i++) {
        uint64_t frame = pmm_alloc_frame();
        if (frame == 0) {
            kprintf("[heap] out of physical memory while extending heap\n");
            return false;
        }
        if (i == 0) first = frame;
        else if (frame != first + i * PAGE_SIZE) {
            kprintf("[heap] non-contiguous frame during extend, stopping growth\n");
            return false;
        }
    }

    size_t region_size = pages * PAGE_SIZE;
    block_header_t *blk = (block_header_t *)first;
    blk->size = region_size - sizeof(block_header_t);
    blk->free = true;
    blk->prev = heap_tail;
    blk->next = NULL;

    if (heap_tail) heap_tail->next = blk;
    if (!heap_head) heap_head = blk;
    heap_tail = blk;
    total_bytes += region_size;
    heap_next_phys = first + region_size;
    return true;
}

void heap_init(uint64_t initial_pages) {
    if (!heap_extend(initial_pages)) {
        kprintf("[heap] FATAL: could not reserve initial heap\n");
    }
    kprintf("[heap] initialized with %lu KiB\n", (unsigned long)(total_bytes / 1024));
}

static void split_block(block_header_t *blk, size_t size) {
    if (blk->size >= size + sizeof(block_header_t) + MIN_SPLIT) {
        block_header_t *new_blk = (block_header_t *)((uint8_t *)(blk + 1) + size);
        new_blk->size = blk->size - size - sizeof(block_header_t);
        new_blk->free = true;
        new_blk->next = blk->next;
        new_blk->prev = blk;
        if (blk->next) blk->next->prev = new_blk;
        blk->next = new_blk;
        if (heap_tail == blk) heap_tail = new_blk;
        blk->size = size;
    }
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN8(size);

    for (int attempt = 0; attempt < 2; attempt++) {
        block_header_t *b = heap_head;
        while (b) {
            if (b->free && b->size >= size) {
                split_block(b, size);
                b->free = false;
                return (void *)(b + 1);
            }
            b = b->next;
        }
        /* nothing fits: grow the heap and retry once */
        size_t pages_needed = (size + sizeof(block_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
        if (pages_needed < 4) pages_needed = 4;
        if (!heap_extend(pages_needed)) break;
    }

    kprintf("[heap] kmalloc(%lu) FAILED\n", (unsigned long)size);
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *blk = (block_header_t *)ptr - 1;
    blk->free = true;

    if (blk->next && blk->next->free) {
        blk->size += sizeof(block_header_t) + blk->next->size;
        blk->next = blk->next->next;
        if (blk->next) blk->next->prev = blk;
        else heap_tail = blk;
    }
    if (blk->prev && blk->prev->free) {
        block_header_t *prev = blk->prev;
        prev->size += sizeof(block_header_t) + blk->size;
        prev->next = blk->next;
        if (blk->next) blk->next->prev = prev;
        else heap_tail = prev;
    }
}

size_t heap_total_bytes(void) { return total_bytes; }

size_t heap_used_bytes(void) {
    size_t used = 0;
    for (block_header_t *b = heap_head; b; b = b->next)
        if (!b->free) used += b->size;
    return used;
}
