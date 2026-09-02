OUTPUT_ARCH(riscv)
ENTRY(main)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS
{
  . = 0x1000;
  .text : ALIGN(4096) { *(.text .text.*) } :text
  .rodata : ALIGN(16) { *(.rodata .rodata.*) *(.srodata .srodata.*) } :text
  . = ALIGN(4096);
  .data : ALIGN(16) { *(.data .data.*) *(.sdata .sdata.*) } :data
  .bss : ALIGN(16) { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } :data
  . = ALIGN(4096);
}
