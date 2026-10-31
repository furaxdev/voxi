.section .rodata

.global hello_elf_start
.global hello_elf_end
.align 16
hello_elf_start:
.incbin "build/userland/hello.elf"
hello_elf_end:

.global forkdemo_elf_start
.global forkdemo_elf_end
.align 16
forkdemo_elf_start:
.incbin "build/userland/forkdemo.elf"
forkdemo_elf_end:
