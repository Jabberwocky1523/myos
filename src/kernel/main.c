#include "lib/method.h"
#include "lock/method.h"

#ifndef LAB_TEST
#define LAB_TEST 1
#endif
#ifndef LOCK_SUM
#define LOCK_SUM 1
#endif

#define ITERATIONS 100000
#define HART_COUNT 2

static volatile uint32 started;
static volatile uint32 finished;
static volatile uint64 sum;
static spinlock_t sum_lock;

#if LAB_TEST == 1
static void test1(void)
{
  printf("Hello world from hart %d\n", (int)hart_id());
}
#else
static void test2(void)
{
  for (int i = 0; i < ITERATIONS; ++i)
  {
#if LOCK_SUM
    spinlock_acquire(&sum_lock);
    sum++;
    spinlock_release(&sum_lock);
#else
    sum++;
#endif
  }

  uint32 done = __sync_add_and_fetch(&finished, 1);
  if (done == HART_COUNT)
    printf("sum = %d (expected %x, lock=%d)\n",
           sum, (uint64)HART_COUNT * ITERATIONS, LOCK_SUM);
}
#endif

int main(void)
{
  uint64 id = hart_id();

  if (id == 0)
  {
    print_init();
    spinlock_init(&sum_lock, "sum");
    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
  }
  else
  {
    while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
      ;
  }

#if LAB_TEST == 2
  test2();
#else
  test1();
#endif

  for (;;)
    asm volatile("wfi");
}
