#include "gdt.h"
#include "../../lib/string.h"

struct tss_t {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* indices: 0 null, 1 kernel code, 2 kernel data, 3 user data, 4 user
 * code, 5-6 TSS (a system descriptor takes two 8-byte slots in long
 * mode). */
static uint64_t gdt[7];
static struct gdt_ptr gdtp;
static struct tss_t tss;

static void set_tss_descriptor(uint64_t base, uint32_t limit) {
    gdt[5] = (uint64_t)(limit & 0xFFFF)
           | ((base & 0xFFFFFF) << 16)
           | (0x89ull << 40)                    /* present, type=9 (64-bit TSS) */
           | (((uint64_t)(limit >> 16) & 0xF) << 48);
    gdt[6] = (base >> 32) & 0xFFFFFFFFull;
}

void gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = 0x00AF9A000000FFFFull; /* kernel code, ring0 */
    gdt[2] = 0x00AF92000000FFFFull; /* kernel data, ring0 */
    gdt[3] = 0x00AFF2000000FFFFull; /* user data,   ring3 */
    gdt[4] = 0x00AFFA000000FFFFull; /* user code,   ring3 */

    memset(&tss, 0, sizeof(tss));
    tss.iomap_base = sizeof(tss);
    set_tss_descriptor((uint64_t)&tss, sizeof(tss) - 1);

    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint64_t)&gdt;

    __asm__ volatile ("lgdt %0" :: "m"(gdtp));

    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        ::: "ax"
    );

    uint16_t tss_sel = 5 * 8;
    __asm__ volatile ("ltr %0" :: "r"(tss_sel));
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
