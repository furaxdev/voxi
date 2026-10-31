#include "vmm.h"
#include "pmm.h"
#include "../lib/string.h"

static uint64_t g_kernel_cr3 = 0;
static uint64_t g_kernel_pdpt_entry0 = 0;

void vmm_init(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_kernel_cr3 = cr3;

    uint64_t *kernel_pml4 = (uint64_t *)(cr3 & ~0xFFFull);
    uint64_t *kernel_pdpt = (uint64_t *)(kernel_pml4[0] & ~0xFFFull);
    g_kernel_pdpt_entry0 = kernel_pdpt[0];
}

uint64_t vmm_kernel_cr3(void) {
    return g_kernel_cr3;
}

address_space_t *vmm_create_address_space(void) {
    /* address_space_t itself is tiny bookkeeping, fine to kmalloc; but
     * kmalloc isn't declared here to keep vmm.c decoupled from the
     * heap -- callers that already depend on mm/heap.h pass it in via
     * a static allocation instead. Simplicity: just use a physical
     * frame's worth of space isn't warranted for 32 bytes, so take it
     * from a tiny static pool. */
    static address_space_t pool[8];
    static int pool_used = 0;
    if (pool_used >= 8) return NULL;
    address_space_t *as = &pool[pool_used++];

    uint64_t pml4 = pmm_alloc_frame();
    uint64_t pdpt = pmm_alloc_frame();
    uint64_t pd   = pmm_alloc_frame();
    uint64_t pt   = pmm_alloc_frame();
    if (!pml4 || !pdpt || !pd || !pt) return NULL;

    memset((void *)pml4, 0, PAGE_SIZE);
    memset((void *)pdpt, 0, PAGE_SIZE);
    memset((void *)pd, 0, PAGE_SIZE);
    memset((void *)pt, 0, PAGE_SIZE);

    uint64_t *pml4p = (uint64_t *)pml4;
    pml4p[0] = pdpt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    uint64_t *pdptp = (uint64_t *)pdpt;
    pdptp[0] = g_kernel_pdpt_entry0;                          /* share 0-1GiB (kernel) */
    pdptp[1] = pd | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;    /* private 1-2GiB window */

    uint64_t *pdp = (uint64_t *)pd;
    pdp[0] = pt | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    as->pml4_phys = pml4;
    as->pdpt_phys = pdpt;
    as->pd_phys = pd;
    as->pt_phys = pt;
    return as;
}

void vmm_map_user_page(address_space_t *as, uint64_t vaddr, uint64_t phys, uint64_t flags) {
    uint64_t index = (vaddr - USER_BASE) / PAGE_SIZE;
    uint64_t *pt = (uint64_t *)as->pt_phys;
    pt[index] = (phys & ~0xFFFull) | flags;
}

uint64_t vmm_get_user_page(address_space_t *as, uint64_t vaddr) {
    uint64_t index = (vaddr - USER_BASE) / PAGE_SIZE;
    uint64_t *pt = (uint64_t *)as->pt_phys;
    uint64_t entry = pt[index];
    return (entry & PAGE_PRESENT) ? (entry & ~0xFFFull) : 0;
}

void vmm_reset_user_window(address_space_t *as) {
    uint64_t *pt = (uint64_t *)as->pt_phys;
    for (int i = 0; i < 512; i++) {
        if (pt[i] & PAGE_PRESENT) {
            pmm_free_frame(pt[i] & ~0xFFFull);
            pt[i] = 0;
        }
    }
}
