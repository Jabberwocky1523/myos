#include "method.h"
#include "../arch/type.h"
#include "../lib/method.h"
#include "../lock/method.h"

#ifndef TIMER_INTERVAL
#define TIMER_INTERVAL 1000000UL
#endif

#define CLINT_MTIMECMP(hart) (0x02004000UL + 8UL * (hart))
#define CLINT_MTIME           0x0200bff8UL

static uint64 timer_scratch[MAX_HARTS][5] __attribute__((aligned(16)));
static spinlock_t ticks_lock;
static uint64 sys_ticks;

void timer_init(void)
{
  uint64 hart;
  asm volatile("csrr %0, mhartid" : "=r"(hart));
  if (hart >= MAX_HARTS)
    for (;;)
      asm volatile("wfi");

  volatile uint64 *mtimecmp = (volatile uint64 *)CLINT_MTIMECMP(hart);
  volatile uint64 *mtime = (volatile uint64 *)CLINT_MTIME;
  *mtimecmp = *mtime + TIMER_INTERVAL;

  timer_scratch[hart][3] = (uint64)mtimecmp;
  timer_scratch[hart][4] = TIMER_INTERVAL;
  asm volatile("csrw mscratch, %0" : : "r"(&timer_scratch[hart][0]));
  asm volatile("csrw mtvec, %0" : : "r"((uint64)timer_vector));
  asm volatile("csrs mie, %0" : : "r"(MIE_MTIE));
}

void timer_create(void)
{
  spinlock_init(&ticks_lock, "ticks");
  sys_ticks = 0;
}

void timer_update(void)
{
  spinlock_acquire(&ticks_lock);
  sys_ticks++;
  spinlock_release(&ticks_lock);
}

uint64 timer_get_ticks(void)
{
  spinlock_acquire(&ticks_lock);
  uint64 ticks = sys_ticks;
  spinlock_release(&ticks_lock);
  return ticks;
}

