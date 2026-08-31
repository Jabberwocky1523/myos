#include "lib/method.h"
#include "lock/method.h"
#include "mem/method.h"
#include "trap/method.h"

static volatile uint32 kernel_ready;

void init(void)
{
  print_init();
  pmem_init();
  kvm_init();
}

static void test_timer_interrupts(void)
{
  uint64 previous = timer_get_ticks();

  for (int observed = 0; observed < 5; ++observed)
  {
    uint64 current;
    do
    {
      current = timer_get_ticks();
    } while (current == previous);
    assert(current > previous, "timer ticks are not monotonic");
    printf("timer tick %x\n", current);
    previous = current;
  }
  printf("test_timer_interrupts passed\n");
}

static void idle_forever(void) __attribute__((noreturn));

static void idle_forever(void)
{
}

int main(void)
{
  uint64 id = hart_id();

  if (id == 0)
  {
    init();
    kvm_inithart();
    trap_kernel_init();
    trap_kernel_inithart();
    test_timer_interrupts();
    printf("lab3 hart %d ready\n", (int)id);
    __atomic_store_n(&kernel_ready, 1, __ATOMIC_RELEASE);
  }
  else
  {
    while (__atomic_load_n(&kernel_ready, __ATOMIC_ACQUIRE) == 0)
      ;
    kvm_inithart();
    trap_kernel_inithart();
    printf("lab3 hart %d ready\n", (int)id);
  }
  for (;;)
    asm volatile("wfi");
}
