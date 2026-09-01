include common.mk

.DEFAULT_GOAL := all
HARTS ?= 2
TIMER_INTERVAL ?= 1000000

ifneq ($(filter $(HARTS),1 2),$(HARTS))
$(error HARTS must be 1 or 2)
endif

BUILD_DIR := target/harts$(HARTS)-timer$(TIMER_INTERVAL)
CFLAGS += -DHART_COUNT=$(HARTS) -DTIMER_INTERVAL=$(TIMER_INTERVAL)
C_SRCS := $(wildcard src/kernel/*.c) $(wildcard src/kernel/boot/*.c) \
	$(wildcard src/kernel/lib/*.c) $(wildcard src/kernel/lock/*.c) \
	$(wildcard src/kernel/mem/*.c) $(wildcard src/kernel/arch/*.c) \
	$(wildcard src/kernel/proc/*.c) $(wildcard src/kernel/syscall/*.c) \
	$(wildcard src/kernel/trap/*.c)
S_SRCS := $(wildcard src/kernel/boot/*.S) $(wildcard src/kernel/arch/*.S) \
	$(wildcard src/kernel/proc/*.S) $(wildcard src/kernel/trap/*.S)
KERNEL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
	$(patsubst src/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))
KERNEL_DEPS := $(KERNEL_OBJS:.o=.d)
USER_OBJ := $(BUILD_DIR)/user/initcode.o
USER_ELF := $(BUILD_DIR)/user/initcode.elf
USER_BIN := $(BUILD_DIR)/user/initcode.bin
INITCODE_H := src/user/initcode.h
INITCODE_DATA_OBJ := $(BUILD_DIR)/user/initcode_data.o
CONFIG_ELF := $(BUILD_DIR)/kernel-qemu.elf

-include $(KERNEL_DEPS) $(USER_OBJ:.o=.d) $(INITCODE_DATA_OBJ:.o=.d)
.PHONY: all run clean FORCE
all: kernel-qemu.elf
kernel-qemu.elf: FORCE $(CONFIG_ELF)
	cp $(CONFIG_ELF) $@
$(CONFIG_ELF): $(KERNEL_OBJS) $(INITCODE_DATA_OBJ) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS) $(INITCODE_DATA_OBJ)
FORCE:
$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<
$(BUILD_DIR)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<
$(USER_OBJ): src/user/initcode.c src/user/sys.h src/user/syscall_arch.h src/user/syscall_num.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I src/user -fno-asynchronous-unwind-tables -MMD -MP -c -o $@ $<
$(USER_ELF): $(USER_OBJ)
	$(LD) -z max-page-size=4096 -Ttext 0x1000 -e main -o $@ $<
$(USER_BIN): $(USER_ELF)
	$(OBJCOPY) -S -O binary $< $@
$(INITCODE_H): $(USER_BIN)
	xxd -i -n initcode $< $@
$(INITCODE_DATA_OBJ): $(INITCODE_H)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -x c -c -o $@ $<
run: kernel-qemu.elf
	$(QEMU) -machine virt -bios none -kernel $< -m 128M -smp $(HARTS) -nographic -serial mon:stdio
clean:
	rm -rf target kernel-qemu.elf $(INITCODE_H)
