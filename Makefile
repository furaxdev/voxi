CC = gcc
LD = ld

BOOT_DIR  = boot
KERNEL_DIR = kernel
ISO_DIR   = iso
BUILD_DIR = build

CFLAGS = -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
         -fno-pic -fno-pie -mno-red-zone -mno-sse -mno-sse2 -mno-mmx \
         -mno-80387 -m64 -Wall -Wextra -I$(KERNEL_DIR)/include -I$(KERNEL_DIR) \
         -g -O2 -MMD -MP
LDFLAGS = -n -T $(BOOT_DIR)/linker.ld -nostdlib -z max-page-size=0x1000

C_SRCS := $(shell find $(KERNEL_DIR) -name '*.c')
S_SRCS := $(BOOT_DIR)/boot.S $(KERNEL_DIR)/arch/x86_64/isr.S $(KERNEL_DIR)/arch/x86_64/switch.S

C_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
S_OBJS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SRCS))
OBJS   := $(S_OBJS) $(C_OBJS)
DEPS   := $(C_OBJS:.o=.d)

KERNEL_BIN = $(BUILD_DIR)/voxi.bin
ISO_IMAGE  = $(BUILD_DIR)/voxi.iso

.PHONY: all clean run run-nographic iso

all: $(KERNEL_BIN)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) $(BOOT_DIR)/linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "Built $@"

iso: $(KERNEL_BIN)
	@mkdir -p $(BUILD_DIR)/isoroot/boot/grub
	cp $(KERNEL_BIN) $(BUILD_DIR)/isoroot/boot/voxi.bin
	cp $(ISO_DIR)/boot/grub/grub.cfg $(BUILD_DIR)/isoroot/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_IMAGE) $(BUILD_DIR)/isoroot

run: iso
	qemu-system-x86_64 -cdrom $(ISO_IMAGE) -serial stdio -m 512M

run-nographic: iso
	qemu-system-x86_64 -cdrom $(ISO_IMAGE) -serial stdio -display none -m 512M

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
