#include "lib/type.h"
#include "arch/type.h"
#include "arch/method.h"
#include "trap/method.h"

extern int main(void);

#define PMP_R            (1UL << 0)
#define PMP_W            (1UL << 1)
#define PMP_X            (1UL << 2)
#define PMP_A_TOR        (1UL << 3)

void start(void)
{
  uint64 hartid;
  uint64 mstatus = r_mstatus();

  /* Keep global interrupt delivery off until S-mode vectors are installed. */
  mstatus &= ~(MSTATUS_MPP_MASK | MSTATUS_FS_MASK |
               MSTATUS_MIE | SSTATUS_SIE);
  mstatus |= MSTATUS_MPP_S | MSTATUS_FS_INITIAL;
  w_mstatus(mstatus);
  w_mepc((uint64)main);

  /* One top-of-range entry ending at the largest representable address. */
  w_pmpaddr0(~0UL);
  w_pmpcfg0(PMP_R | PMP_W | PMP_X | PMP_A_TOR);

  /* Route exceptions and supervisor interrupts to S-mode. */
  w_medeleg(0xffffUL);
  w_mideleg(0xffffUL);
  w_sie(r_sie() | SIE_SSIE | SIE_SEIE);

  /* Machine timer interrupts are converted to supervisor software traps. */
  timer_init();

  w_satp(0);
  sfence_vma();
  hartid = r_mhartid();
  w_tp(hartid);
  asm volatile("mret");
  __builtin_unreachable();
}
