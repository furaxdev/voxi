#include "elf.h"
#include "mm/pmm.h"
#include "lib/string.h"
#include "lib/kprintf.h"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD 1

bool elf_load(const uint8_t *data, size_t size, address_space_t *as, uint64_t *entry_out) {
    if (size < sizeof(elf64_ehdr_t)) return false;
    const elf64_ehdr_t *eh = (const elf64_ehdr_t *)data;

    if (eh->e_ident[0] != 0x7f || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        kprintf("[elf] not an ELF file\n");
        return false;
    }

    const elf64_phdr_t *phdrs = (const elf64_phdr_t *)(data + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        const elf64_phdr_t *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        if (ph->p_vaddr < USER_BASE || ph->p_vaddr + ph->p_memsz > USER_BASE + 0x200000ull) {
            kprintf("[elf] segment %d (vaddr=%p, size=%lu) is outside the mapped window\n",
                    i, (void *)ph->p_vaddr, (unsigned long)ph->p_memsz);
            return false;
        }

        uint64_t page_start = ph->p_vaddr & ~0xFFFull;
        uint64_t page_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFFull;

        for (uint64_t va = page_start; va < page_end; va += PAGE_SIZE) {
            uint64_t phys = pmm_alloc_frame();
            if (!phys) {
                kprintf("[elf] out of physical memory while loading\n");
                return false;
            }
            memset((void *)phys, 0, PAGE_SIZE);

            uint64_t seg_file_start = ph->p_vaddr;
            uint64_t seg_file_end = ph->p_vaddr + ph->p_filesz;
            uint64_t copy_start = va > seg_file_start ? va : seg_file_start;
            uint64_t copy_end = (va + PAGE_SIZE) < seg_file_end ? (va + PAGE_SIZE) : seg_file_end;

            if (copy_end > copy_start) {
                uint64_t file_off = ph->p_offset + (copy_start - ph->p_vaddr);
                uint64_t page_off = copy_start - va;
                memcpy((void *)(phys + page_off), data + file_off, copy_end - copy_start);
            }

            vmm_map_user_page(as, va, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        }
    }

    *entry_out = eh->e_entry;
    return true;
}
