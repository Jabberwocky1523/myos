#include "method.h"
#include "../arch/method.h"
#include "../proc/method.h"
#include "../lib/method.h"
extern void panic(const char *s) __attribute__((noreturn));

uint64 hart_id(void) { return r_tp(); }

void push_off(void)
{
  int old = intr_get();
  intr_off();
  cpu_t *cpu = mycpu();
  if (cpu == 0)
    cpu->origin = old;
  cpu->noff++;
}

void pop_off(void)
{
  cpu_t *cpu = mycpu();
  assert(intr_get() == 0, "push_off: 1\n"); // 确保此时中断是关闭的
  assert(cpu->noff >= 1, "push_off: 2\n");  // 确保push和pop的对应
  cpu->noff--;
  if (cpu->noff == 0 && cpu->origin == 1) // 只有所有push操作都被抵消且原来状态是开着时
    intr_on();
}

void spinlock_init(spinlock_t *lk, const char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->owner = ~0UL;
}

bool spinlock_holding(spinlock_t *lk)
{
  return lk->locked != 0 && lk->owner == hart_id();
}

/* Acquire a spinlock and diagnose recursive acquisition by lock name. */
void spinlock_acquire(spinlock_t *lk)
{
  push_off();
  if (spinlock_holding(lk))
    panic(lk->name);

  while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;
  __sync_synchronize();
  lk->owner = hart_id();
}

/* Release a spinlock and diagnose ownership errors by lock name. */
void spinlock_release(spinlock_t *lk)
{
  if (!spinlock_holding(lk))
    panic(lk->name);

  lk->owner = ~0UL;
  __sync_synchronize();
  __sync_lock_release(&lk->locked);
  pop_off();
}
