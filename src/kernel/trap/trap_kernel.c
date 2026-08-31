#include "method.h"
#include "../arch/type.h"
#include "../lib/method.h"

void trap_kernel_init(void)
{
  asm volatile("csrw stvec, %0" : : "r"((uint64)kernel_vector));
  timer_create();
  plic_init();
  uart_enable_rx_interrupt();
}

void trap_kernel_inithart(void)
{
  asm volatile("csrw stvec, %0" : : "r"((uint64)kernel_vector));
  plic_inithart();
  asm volatile("csrs sie, %0" : : "r"(SIE_SSIE | SIE_SEIE));
  asm volatile("csrs sstatus, %0" : : "r"(SSTATUS_SIE) : "memory");
}

void timer_interrupt_handler(void)
{
  timer_update();
}

void external_interrupt_handler(void)
{
  int irq = plic_claim();

  if (irq == UART0_IRQ) {
    uart_intr();
  } else if (irq != 0) {
    printf("unexpected PLIC irq %d\n", irq);
  }

  if (irq != 0)
    plic_complete(irq);
}

void trap_kernel_handler(void)
{
  uint64 scause = csr_read_scause();

  if ((scause & SCAUSE_INTERRUPT) != 0) {
    uint64 code = scause & ~SCAUSE_INTERRUPT;
    if (code == SCAUSE_S_SOFTWARE) {
      asm volatile("csrc sip, %0" : : "r"(SIP_SSIP) : "memory");
      timer_interrupt_handler();
      return;
    }
    if (code == SCAUSE_S_EXTERNAL) {
      external_interrupt_handler();
      return;
    }
  }

  printf("unexpected kernel trap: scause=%x sepc=%x stval=%x\n",
         scause, csr_read_sepc(), csr_read_stval());
  panic("unexpected kernel trap");
}

