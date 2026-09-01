TOOLPREFIX ?= riscv64-linux-gnu-

CC      := $(TOOLPREFIX)gcc
LD      := $(TOOLPREFIX)ld
OBJCOPY := $(TOOLPREFIX)objcopy
OBJDUMP := $(TOOLPREFIX)objdump

QEMU ?= qemu-system-riscv64

CFLAGS := -std=gnu11 -Wall -Wextra -Werror -O1 -freorder-functions -g \
	-ffreestanding -fno-common \
	-fno-builtin -fno-omit-frame-pointer -fno-pie -fno-stack-protector \
	-mcmodel=medany -march=rv64gc -mabi=lp64d -nostdlib -nostartfiles \
	-I src/kernel
LDFLAGS := -T kernel.ld -z max-page-size=4096
