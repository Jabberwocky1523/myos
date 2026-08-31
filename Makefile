include common.mk

.DEFAULT_GOAL := all

HARTS ?= 2
TIMER_INTERVAL ?= 1000000

ifneq ($(filter $(HARTS),1 2 3 4 5 6 7 8),$(HARTS))
$(error HARTS must be an integer from 1 to 8)
endif

CFLAGS += -DHART_COUNT=$(HARTS) -DTIMER_INTERVAL=$(TIMER_INTERVAL)
BUILD_DIR := target/harts$(HARTS)-timer$(TIMER_INTERVAL)

C_SRCS := $(wildcard src/kernel/*.c) \
	$(wildcard src/kernel/boot/*.c) \
	$(wildcard src/kernel/lib/*.c) \
	$(wildcard src/kernel/lock/*.c) \
	$(wildcard src/kernel/mem/*.c) \
	$(wildcard src/kernel/trap/*.c)
S_SRCS := $(wildcard src/kernel/boot/*.S) \
	$(wildcard src/kernel/trap/*.S)
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
	$(patsubst src/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))
DEPS := $(OBJS:.o=.d)
CONFIG_ELF := $(BUILD_DIR)/kernel-qemu.elf

-include $(DEPS)

.PHONY: all run clean FORCE

all: kernel-qemu.elf

kernel-qemu.elf: FORCE $(CONFIG_ELF)
	cp $(CONFIG_ELF) $@

$(CONFIG_ELF): $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

FORCE:

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

run: kernel-qemu.elf
	$(QEMU) -machine virt -bios none -kernel $< -m 128M -smp $(HARTS) \
		-nographic -serial mon:stdio

clean:
	rm -rf target kernel-qemu.elf

