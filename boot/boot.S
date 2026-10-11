/*
 * Voxi kernel boot entry.
 * Loaded by GRUB via Multiboot2 in 32-bit protected mode.
 * Sets up identity-mapped paging for the first 1 GiB (2 MiB pages),
 * enables long mode, and jumps into the 64-bit kernel entry point.
 */
.intel_syntax noprefix

.set MB2_MAGIC, 0xe85250d6
.set MB2_ARCH,  0

.section .multiboot2
.align 8
multiboot_header:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long header_end - multiboot_header
    .long -(MB2_MAGIC + MB2_ARCH + (header_end - multiboot_header))
    /* end tag */
    .align 8
    .word 0
    .word 0
    .long 8
header_end:

.section .bss
.align 4096
pml4: .skip 4096
pdpt: .skip 4096
pd:   .skip 4096
.align 16
stack_bottom: .skip 65536
stack_top:

.section .rodata
.align 8
gdt64:
    .quad 0                          /* null descriptor */
    .quad 0x00AF9A000000FFFF         /* 0x08: 64-bit code, ring0 */
    .quad 0x00AF92000000FFFF         /* 0x10: 64-bit data, ring0 */
gdt64_ptr:
    .word gdt64_ptr - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global _start
_start:
    cli
    lea esp, [stack_top]
    mov esi, ebx            /* save multiboot2 info pointer */

    /* zero the page table region */
    lea edi, [pml4]
    mov ecx, (4096*3)/4
    xor eax, eax
    rep stosd

    /* pml4[0] -> pdpt, present+writable+user (U/S must be set at every
     * level of the walk for a page to be user-accessible) */
    lea eax, [pdpt]
    or eax, 0x7
    lea edi, [pml4]
    mov [edi], eax

    /* pdpt[0] -> pd, present+writable+user */
    lea eax, [pd]
    or eax, 0x7
    lea edi, [pdpt]
    mov [edi], eax

    /* fill page directory: 512 entries x 2MiB huge pages = 1GiB identity map */
    lea edi, [pd]
    xor ecx, ecx
fill_pd:
    mov edx, ecx
    shl edx, 21
    or edx, 0x87          /* present + writable + user-accessible + huge(PS) */
    mov [edi], edx
    mov dword ptr [edi+4], 0
    add edi, 8
    inc ecx
    cmp ecx, 512
    jl fill_pd

    /* load PML4 */
    lea eax, [pml4]
    mov cr3, eax

    /* enable PAE */
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    /* enable long mode (EFER.LME) */
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    /* enable paging + protected mode */
    mov eax, cr0
    or eax, (1 << 31) | 1
    mov cr0, eax

    lgdt [gdt64_ptr]

    mov ebx, esi          /* carry multiboot ptr across the far jump */

    /* far jump into 64-bit code segment via retf trick */
    lea eax, [long_mode_start]
    push 0x08
    push eax
    retf

.code64
long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    lea rsp, [stack_top]
    xor rdi, rdi
    mov edi, ebx           /* multiboot2 info pointer, zero-extended */
    call kernel_main

hang:
    cli
    hlt
    jmp hang
