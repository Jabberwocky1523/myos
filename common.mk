TOOLPREFIX ?= riscv64-linux-gnu-

CC      := $(TOOLPREFIX)gcc
LD      := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

QEMU ?= qemu-system-riscv64

CFLAGS := -std=gnu11 -Wall -Wextra -Werror -O2 -g -ffreestanding -fno-common \
	-fno-builtin -fno-omit-frame-pointer -mcmodel=medany -march=rv64gc \
	-mabi=lp64d -nostdlib -nostartfiles -I src/kernel
LDFLAGS := -T kernel.ld -z max-page-size=4096

