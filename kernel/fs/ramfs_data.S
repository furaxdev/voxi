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

.global pipedemo_elf_start
.global pipedemo_elf_end
.align 16
pipedemo_elf_start:
.incbin "build/userland/pipedemo.elf"
pipedemo_elf_end:

.global spin_elf_start
.global spin_elf_end
.align 16
spin_elf_start:
.incbin "build/userland/spin.elf"
spin_elf_end:

.global libctest_elf_start
.global libctest_elf_end
.align 16
libctest_elf_start:
.incbin "build/userland/libctest.elf"
libctest_elf_end:

.global sigdemo_elf_start
.global sigdemo_elf_end
.align 16
sigdemo_elf_start:
.incbin "build/userland/sigdemo.elf"
sigdemo_elf_end:
