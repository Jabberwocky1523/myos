include common.mk

.DEFAULT_GOAL := all
HARTS ?= 2
TIMER_INTERVAL ?= 1000000

ifneq ($(filter $(HARTS),1 2),$(HARTS))
$(error HARTS must be 1 or 2)
endif

BUILD_DIR := target/harts$(HARTS)-timer$(TIMER_INTERVAL)
CFLAGS += -DHART_COUNT=$(HARTS) -DTIMER_INTERVAL=$(TIMER_INTERVAL)
USER_CFLAGS := $(CFLAGS) -Wno-sign-compare -Wno-unused-parameter
C_SRCS := $(wildcard src/kernel/*.c) $(wildcard src/kernel/boot/*.c) \
	$(wildcard src/kernel/lib/*.c) $(wildcard src/kernel/lock/*.c) \
	$(wildcard src/kernel/mem/*.c) $(wildcard src/kernel/arch/*.c) \
	$(wildcard src/kernel/proc/*.c) $(wildcard src/kernel/syscall/*.c) \
	$(wildcard src/kernel/trap/*.c) $(wildcard src/kernel/fs/*.c)
S_SRCS := $(wildcard src/kernel/boot/*.S) $(wildcard src/kernel/arch/*.S) \
	$(wildcard src/kernel/proc/*.S) $(wildcard src/kernel/trap/*.S)
KERNEL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
	$(patsubst src/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)

INITCODE_OBJ := $(BUILD_DIR)/user/initcode.o
INITCODE_ELF := $(BUILD_DIR)/user/initcode.elf
INITCODE_BIN := $(BUILD_DIR)/user/initcode.bin
INITCODE_H := src/user/initcode.h
INITCODE_DATA_OBJ := $(BUILD_DIR)/user/initcode_data.o

USER_NAMES := test_1 test_2 test_3 test_4
USER_COMMON_OBJS := $(BUILD_DIR)/user/help.o $(BUILD_DIR)/user/syscall.o
USER_TEST_OBJS := $(addprefix $(BUILD_DIR)/user/,$(addsuffix .o,$(USER_NAMES)))
USER_ELFS := $(addprefix $(BUILD_DIR)/user/,$(USER_NAMES))

CONFIG_ELF := $(BUILD_DIR)/kernel-qemu.elf
MKFS_BIN := $(BUILD_DIR)/mkfs
DISK_IMAGE := disk.img

-include $(KERNEL_DEPS) $(INITCODE_OBJ:.o=.d) $(USER_COMMON_OBJS:.o=.d) \
	$(USER_TEST_OBJS:.o=.d) $(INITCODE_DATA_OBJ:.o=.d)

.PHONY: all run clean FORCE
all: kernel-qemu.elf $(DISK_IMAGE)

kernel-qemu.elf: FORCE $(CONFIG_ELF)
	cp $(CONFIG_ELF) $@

$(CONFIG_ELF): $(KERNEL_OBJS) $(INITCODE_DATA_OBJ) src/loader/kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(INITCODE_DATA_OBJ)

FORCE:

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(INITCODE_OBJ): src/user/initcode.c src/user/sys.h src/user/syscall_arch.h src/user/syscall_num.h
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I src/user -fno-asynchronous-unwind-tables -MMD -MP -c -o $@ $<

$(INITCODE_ELF): $(INITCODE_OBJ) src/loader/user.ld
	$(LD) -z max-page-size=4096 -T src/loader/user.ld -o $@ $<

$(INITCODE_BIN): $(INITCODE_ELF)
	$(OBJCOPY) -S -O binary $< $@

$(INITCODE_H): $(INITCODE_BIN)
	xxd -i -n initcode $< $@

$(INITCODE_DATA_OBJ): $(INITCODE_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -x c -c -o $@ $<

$(BUILD_DIR)/user/%.o: src/user/%.c src/user/help.h src/user/syscall_num.h
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -I src/user -fno-asynchronous-unwind-tables -MMD -MP -c -o $@ $<

$(BUILD_DIR)/user/%: $(BUILD_DIR)/user/%.o $(USER_COMMON_OBJS) src/loader/user.ld
	$(LD) -z max-page-size=4096 -T src/loader/user.ld -o $@ $< $(USER_COMMON_OBJS)

$(MKFS_BIN): src/mkfs/mkfs.c src/mkfs/mkfs.h
	@mkdir -p $(dir $@)
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -O2 -o $@ src/mkfs/mkfs.c

$(DISK_IMAGE): $(MKFS_BIN) $(USER_ELFS)
	$(MKFS_BIN) $@ $(USER_ELFS)

run: kernel-qemu.elf $(DISK_IMAGE)
	$(QEMU) -machine virt -bios none -kernel $< -m 128M -smp $(HARTS) \
		-drive file=$(DISK_IMAGE),if=none,format=raw,id=x0 \
		-device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
		-nographic -serial mon:stdio

clean:
	rm -rf target kernel-qemu.elf $(INITCODE_H) $(DISK_IMAGE)
