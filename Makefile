include common.mk

TEST ?= 1
LOCK_SUM ?= 1
CFLAGS += -DLAB_TEST=$(TEST) -DLOCK_SUM=$(LOCK_SUM)
BUILD_DIR := target/test$(TEST)-lock$(LOCK_SUM)

C_SRCS := $(wildcard src/kernel/*.c) \
	$(wildcard src/kernel/boot/*.c) \
	$(wildcard src/kernel/lib/*.c) \
	$(wildcard src/kernel/lock/*.c)
S_SRCS := $(wildcard src/kernel/boot/*.S)
OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
	$(patsubst src/%.S,$(BUILD_DIR)/%.o,$(S_SRCS))

.PHONY: all run clean

all: kernel-qemu.elf

kernel-qemu.elf: $(OBJS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

run: kernel-qemu.elf
	$(QEMU) -machine virt -bios none -kernel $< -m 128M -smp 2 \
		-nographic -serial mon:stdio

clean:
	rm -rf target kernel-qemu.elf
