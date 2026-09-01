#include "lib/method.h"
#include "lock/method.h"
#include "mem/method.h"
#include "proc/method.h"
#include "trap/method.h"

static volatile uint32 kernel_ready;
volatile static bool over_1 = false, over_2 = false;
volatile static bool over_3 = false, over_4 = false;
void init(void)
{
  print_init();
  pmem_init();
  kvm_init();
  mmap_init();
  proc_init();
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
    printf("lab5 hart %d ready\n", (int)id);
    mmap_show_nodelist();
    __atomic_store_n(&kernel_ready, 1, __ATOMIC_RELEASE);
    proc_make_first();
  }
  else
  {
    while (__atomic_load_n(&kernel_ready, __ATOMIC_ACQUIRE) == 0)
      ;
    kvm_inithart();
    trap_kernel_inithart();
    printf("lab5 hart %d ready\n", (int)id);
  }
  for (;;)
    ;
}
