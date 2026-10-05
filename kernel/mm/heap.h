#ifndef VOXI_HEAP_H
#define VOXI_HEAP_H

#include <voxi/types.h>

void heap_init(uint64_t initial_pages);
void *kmalloc(size_t size);
void kfree(void *ptr);
size_t heap_used_bytes(void);
size_t heap_total_bytes(void);

#endif
