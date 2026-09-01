#include "method.h"
#include "../arch/method.h"
#include "../arch/type.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../syscall/method.h"

extern char trampoline[];
extern char user_vector[];
extern char user_return[];

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
    syscall();
  }
  else if (scause == SCAUSE_LOAD_PAGE_FAULT ||
           scause == SCAUSE_STORE_PAGE_FAULT)
  {
    printf("user page fault scause=%x stval=%x oldpages=%d\n",
           scause, r_stval(), (int)p->ustack_npage);
    int64 pages = uvm_ustack_grow(p->pgtbl, p->ustack_npage, r_stval());
    if (pages < 0)
    {
      printf("invalid user page fault: scause=%x sepc=%x stval=%x\n",
             scause, r_sepc(), r_stval());
      proc_exit(-1);
    }
    p->ustack_npage = (uint64)pages;
    printf("user stack grown pages=%d\n", (int)pages);
  }
  else
  {
    int which = interrupt_info();
    if (which == 0)
    {
      printf("unexpected user trap: scause=%x sepc=%x stval=%x\n",
             scause, r_sepc(), r_stval());
      proc_exit(-1);
    }
    if (which == 1)
      proc_yield();
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
