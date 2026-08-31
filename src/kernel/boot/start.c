#include "lib/type.h"

extern int main(void);

#define MSTATUS_MPP_MASK (3UL << 11)
#define MSTATUS_MPP_S    (1UL << 11)
#define MSTATUS_FS_MASK  (3UL << 13)
#define MSTATUS_FS_INITIAL (1UL << 13)
#define PMP_R            (1UL << 0)
#define PMP_W            (1UL << 1)
#define PMP_X            (1UL << 2)
#define PMP_A_TOR        (1UL << 3)

static inline uint64 read_mstatus(void)
{
  uint64 value;
  asm volatile("csrr %0, mstatus" : "=r"(value));
  return value;
}

static inline void write_mstatus(uint64 value)
{
  asm volatile("csrw mstatus, %0" : : "r"(value));
}

void start(void)
{
  uint64 hartid;
  uint64 mstatus = read_mstatus();

  mstatus &= ~(MSTATUS_MPP_MASK | MSTATUS_FS_MASK);
  mstatus |= MSTATUS_MPP_S | MSTATUS_FS_INITIAL;
  write_mstatus(mstatus);
  asm volatile("csrw mepc, %0" : : "r"((uint64)main));

  /* One top-of-range entry ending at the largest representable address. */
  asm volatile("csrw pmpaddr0, %0" : : "r"(~0UL));
  asm volatile("csrw pmpcfg0, %0" : :
               "r"(PMP_R | PMP_W | PMP_X | PMP_A_TOR));

  asm volatile("csrw satp, zero");
  asm volatile("sfence.vma zero, zero");
  asm volatile("csrr %0, mhartid" : "=r"(hartid));
  asm volatile("mv tp, %0" : : "r"(hartid));
  asm volatile("mret");
  __builtin_unreachable();
}
