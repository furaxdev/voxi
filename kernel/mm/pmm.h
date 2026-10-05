#ifndef VOXI_PMM_H
#define VOXI_PMM_H

#include <voxi/types.h>

#define PAGE_SIZE 4096

void pmm_init(uint64_t multiboot_addr, uint64_t kernel_end);
uint64_t pmm_alloc_frame(void);
void pmm_free_frame(uint64_t phys_addr);
uint64_t pmm_total_frames(void);
uint64_t pmm_free_frames(void);

#endif
