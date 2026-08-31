OUTPUT_ARCH(riscv)
ENTRY(_entry)

PHDRS
{
  text PT_LOAD FLAGS(5); /* read + execute */
  data PT_LOAD FLAGS(6); /* read + write */
}

SECTIONS
{
  . = 0x80000000;

  .text : ALIGN(4096) {
    *(.text.entry)
    *(.text .text.*)
  } :text

  .rodata : ALIGN(16) {
    *(.rodata .rodata.*)
  } :text

  .data : ALIGN(16) {
    *(.data .data.*)
  } :data

  .bss : ALIGN(16) {
    *(.bss.stack)
    *(.bss .bss.*)
    *(COMMON)
  } :data

  PROVIDE(kernel_end = .);
}
