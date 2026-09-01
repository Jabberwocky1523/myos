#include "method.h"
#include "../arch/method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../trap/method.h"

extern char trampoline[];
extern unsigned char initcode[];
extern unsigned int initcode_len;

cpu_t cpus[MAX_HARTS];
proc_t proczero;

cpu_t *mycpu(void)
{
  uint64 id = hart_id();
  if (id >= MAX_HARTS)
    panic("mycpu: invalid hart id");
  return &cpus[id];
}

proc_t *myproc(void)
{
  push_off();
  proc_t *p = mycpu()->proc;
  pop_off();
  return p;
}

void proc_init(void)
{
  memset(cpus, 0, sizeof(cpus));
  memset(&proczero, 0, sizeof(proczero));
  spinlock_init(&proczero.lock, "proczero");
  proczero.state = PROC_UNUSED;
  proczero.name = "proczero";
}

pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
  pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
  vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline,
              PAGE_SIZE, PTE_R | PTE_X);
  vm_mappages(pgtbl, TRAPFRAME, trapframe,
              PAGE_SIZE, PTE_R | PTE_W);
  return pgtbl;
}

void proc_make_first(void)
{
  proc_t *p = &proczero;
  spinlock_acquire(&p->lock);
  assert(p->state == PROC_UNUSED, "proczero already initialized");

  p->trapframe = (user_trapframe_t *)pmem_alloc(true);
  p->pgtbl = proc_pgtbl_init((uint64)p->trapframe);
  p->kstack = KSTACK(0);

  uint64 code_page = pmem_alloc(false);
  uint64 stack_page = pmem_alloc(false);
  uint64 initcode_size = (uint64)initcode_len;
  assert(initcode_size != 0 && initcode_size <= PAGE_SIZE,
         "initcode must fit in one page");
  memmove((void *)code_page, initcode, (uint32)initcode_size);

  vm_mappages(p->pgtbl, USER_BASE, code_page, PAGE_SIZE,
              PTE_R | PTE_X | PTE_U);
  vm_mappages(p->pgtbl, USER_STACK, stack_page, PAGE_SIZE,
              PTE_R | PTE_W | PTE_U);

  p->trapframe->epc = USER_BASE;
  p->trapframe->sp = USER_STACK_TOP;
  p->heap_top = USER_HEAP_BASE;
  p->ustack_npage = 1;
  p->mmap = NULL;
  p->context.ra = (uint64)proc_return;
  p->context.sp = p->kstack + PAGE_SIZE;
  p->state = PROC_RUNNING;

  cpu_t *c = mycpu();
  c->proc = p;
  swtch(&c->context, &p->context);

  c->proc = NULL;
  spinlock_release(&p->lock);
  panic("proczero stopped");
}

void proc_return(void)
{
  proc_t *p = myproc();
  if (p == NULL || !spinlock_holding(&p->lock))
    panic("proc_return without process lock");
  spinlock_release(&p->lock);
  trap_user_return();
  panic("trap_user_return returned");
}
