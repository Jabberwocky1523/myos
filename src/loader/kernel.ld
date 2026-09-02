OUTPUT_ARCH(riscv)
ENTRY(_entry)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS
{
  . = 0x80000000;
  .text : ALIGN(4096) { *(.text.entry) *(.text .text.*) } :text
  . = ALIGN(4096);
  trampoline = .;
  .trampsec : { *(.trampsec) } :text
  trampoline_end = .;
  ASSERT(trampoline_end - trampoline <= 4096, "trampoline exceeds one page")
  .rodata : ALIGN(16) { *(.rodata .rodata.*) *(.srodata .srodata.*) } :text
  . = ALIGN(4096);
  KERNEL_DATA = .;
  .data : ALIGN(16) { *(.data .data.*) *(.sdata .sdata.*) } :data
  .bss : ALIGN(16) { *(.bss.stack) *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } :data
  . = ALIGN(4096);
  kernel_end = .;
  ALLOC_BEGIN = .;
  ALLOC_END = 0x88000000;
  ASSERT(ALLOC_BEGIN < ALLOC_END, "kernel image exceeds QEMU RAM")
}
