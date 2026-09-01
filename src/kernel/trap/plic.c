#include "method.h"
#include "../lock/method.h"

#define PLIC_BASE 0x0c000000UL
#define PLIC_PRIORITY(irq) (PLIC_BASE + (uint64)(irq) * 4)
#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2080UL + (uint64)(hart) * 0x100)
#define PLIC_STHRESHOLD(hart) (PLIC_BASE + 0x201000UL + (uint64)(hart) * 0x2000)
#define PLIC_SCLAIM(hart) (PLIC_BASE + 0x201004UL + (uint64)(hart) * 0x2000)

void plic_init(void)
{
  *(volatile uint32 *)PLIC_PRIORITY(UART0_IRQ) = 1;
  *(volatile uint32 *)PLIC_PRIORITY(VIRTIO0_IRQ) = 1;
}

void plic_inithart(void)
{
  uint64 hart = hart_id();
  *(volatile uint32 *)PLIC_SENABLE(hart) =
      (1U << UART0_IRQ) | (1U << VIRTIO0_IRQ);
  *(volatile uint32 *)PLIC_STHRESHOLD(hart) = 0;
}

int plic_claim(void)
{
  return (int)*(volatile uint32 *)PLIC_SCLAIM(hart_id());
}

void plic_complete(int irq)
{
  *(volatile uint32 *)PLIC_SCLAIM(hart_id()) = (uint32)irq;
}
