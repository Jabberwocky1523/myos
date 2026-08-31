#include "method.h"
#include "../arch/method.h"
#include "../arch/type.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../../user/syscall_num.h"

extern char trampoline[];
extern char user_vector[];
extern char user_return[];

static void syscall_dispatch(proc_t *p)
{
  switch (p->trapframe->a7)
  {
    case SYS_helloworld:
      printf("%s hello world\n", p->name);
      p->trapframe->a0 = 0;
      break;
    default:
      printf("%s: unknown syscall %d\n", p->name,
             (int)p->trapframe->a7);
      p->trapframe->a0 = (uint64)-1;
      break;
  }
}

void trap_user_handler(void)
{
  proc_t *p = myproc();
  if (p == NULL)
    panic("user trap without process");
  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("user trap did not originate in U-mode");

  w_stvec((uint64)kernel_vector);
  p->trapframe->epc = r_sepc();

  uint64 scause = r_scause();
  if (scause == SCAUSE_U_ECALL)
  {
    p->trapframe->epc += 4;
    syscall_dispatch(p);
  }
  else if (interrupt_info() == 0)
  {
    printf("unexpected user trap: scause=%x sepc=%x stval=%x\n",
           scause, r_sepc(), r_stval());
    panic("unexpected user trap");
  }

  trap_user_return();
}

void trap_user_return(void)
{
  proc_t *p = myproc();
  if (p == NULL || p->trapframe == NULL || p->pgtbl == NULL)
    panic("invalid process return state");

  intr_off();
  uint64 uservec = TRAMPOLINE +
                   ((uint64)user_vector - (uint64)trampoline);
  w_stvec(uservec);

  p->trapframe->kernel_satp = MAKE_SATP(kernel_pgtbl);
  p->trapframe->kernel_sp = p->kstack + PAGE_SIZE;
  p->trapframe->kernel_trap = (uint64)trap_user_handler;
  p->trapframe->kernel_hartid = r_tp();

  uint64 status = r_sstatus();
  status &= ~SSTATUS_SPP;
  status |= SSTATUS_SPIE;
  w_sstatus(status);
  w_sepc(p->trapframe->epc);

  uint64 userret = TRAMPOLINE +
                   ((uint64)user_return - (uint64)trampoline);
  void (*return_to_user)(uint64, uint64) =
      (void (*)(uint64, uint64))userret;
  return_to_user(TRAPFRAME, MAKE_SATP(p->pgtbl));
  panic("user_return returned");
}
