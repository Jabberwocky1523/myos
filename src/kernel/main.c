#include "lib/method.h"
#include "lock/method.h"
#include "mem/method.h"
#include "proc/method.h"
#include "trap/method.h"
#include "fs/method.h"

static volatile uint32 kernel_ready;
/* Initialize global kernel subsystems on the boot hart. */
void init(void)
{
  print_init();
  pmem_init();
  kvm_init();
  mmap_init();
  proc_init();
}

/* Start the requested harts and enter the process scheduler. */
int main(void)
{
  uint64 id = hart_id();

  if (id == 0)
  {
    init();
    kvm_inithart();
    trap_kernel_init();
    trap_kernel_inithart();
    virtio_disk_init();
    printf("lab9 hart %d ready\n", (int)id);
    proc_make_first();
    __atomic_store_n(&kernel_ready, 1, __ATOMIC_RELEASE);
  }
  else
  {
    while (__atomic_load_n(&kernel_ready, __ATOMIC_ACQUIRE) == 0)
      ;
    kvm_inithart();
    trap_kernel_inithart();
    printf("lab9 hart %d ready\n", (int)id);
  }
  proc_scheduler();
}
