.section .rodata
.global hello_elf_start
.global hello_elf_end
.align 16
hello_elf_start:
.incbin "build/userland/hello.elf"
hello_elf_end:
