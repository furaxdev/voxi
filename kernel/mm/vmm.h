#ifndef VOXI_VMM_H
#define VOXI_VMM_H

#include <voxi/types.h>

/* Every user process's private window starts here. It's exactly 1 GiB,
 * right above the kernel's own shared 0-1GiB identity map, which is why
 * it lines up on a PDPT-entry boundary and a single extra PDPT entry is
 * enough to carve it out. Only the first 2 MiB of it is actually backed
 * (one PD entry / one page table) -- plenty for a tiny demo binary. */
#define USER_BASE 0x40000000ull

#define PAGE_PRESENT 0x1ull
#define PAGE_WRITE   0x2ull
#define PAGE_USER    0x4ull

typedef struct {
    uint64_t pml4_phys;
    uint64_t pdpt_phys;
    uint64_t pd_phys;
    uint64_t pt_phys; /* covers USER_BASE .. USER_BASE+2MiB */
} address_space_t;

/* Captures the kernel's own CR3 and the PDPT entry that maps its shared
 * 0-1GiB identity region, so every later address space can reuse it.
 * Call once, early, before any task (and therefore any default cr3) is
 * created. */
void vmm_init(void);

uint64_t vmm_kernel_cr3(void);

/* Builds a fresh address space: PML4[0] is a brand-new PDPT whose own
 * entry 0 is copied from the kernel's (so the kernel, IDT, drivers,
 * everything below 1GiB, is mapped identically and stays usable the
 * instant CR3 switches to this), and whose entry 1 is a private
 * PD+PT covering [USER_BASE, USER_BASE+2MiB) that belongs to no one
 * else. */
address_space_t *vmm_create_address_space(void);

/* Maps one 4KiB page of `phys` at `vaddr` within [USER_BASE,
 * USER_BASE+2MiB) in this address space. */
void vmm_map_user_page(address_space_t *as, uint64_t vaddr, uint64_t phys, uint64_t flags);

#endif
