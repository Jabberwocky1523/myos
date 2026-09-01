#include "method.h"
#include "../arch/method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../trap/method.h"
#include "../fs/method.h"

extern char trampoline[];
extern unsigned char initcode[];
extern unsigned int initcode_len;

cpu_t cpus[MAX_HARTS];
proc_t proc_list[NPROC];
proc_t *proczero;

static spinlock_t pid_lock;
static spinlock_t wait_lock;
static int next_pid = 1;

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

static int alloc_pid(void)
{
  spinlock_acquire(&pid_lock);
  int pid = next_pid++;
  spinlock_release(&pid_lock);
  return pid;
}

void proc_init(void)
{
  memset(cpus, 0, sizeof(cpus));
  spinlock_init(&pid_lock, "pid");
  spinlock_init(&wait_lock, "wait");
  for (uint32 i = 0; i < NPROC; ++i)
  {
    proc_t *p = &proc_list[i];
    memset(p, 0, sizeof(*p));
    spinlock_init(&p->lock, "proc");
    p->state = PROC_UNUSED;
    p->kstack = KSTACK(i);
  }
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

proc_t *proc_alloc(void)
{
  for (uint32 i = 0; i < NPROC; ++i)
  {
    proc_t *p = &proc_list[i];
    spinlock_acquire(&p->lock);
    if (p->state != PROC_UNUSED)
    {
      spinlock_release(&p->lock);
      continue;
    }
    p->state = PROC_EMBRYO;
    p->pid = alloc_pid();
    p->trapframe = (user_trapframe_t *)pmem_try_alloc(true);
    if (p->trapframe == NULL)
    {
      p->pid = 0;
      p->state = PROC_UNUSED;
      spinlock_release(&p->lock);
      return NULL;
    }
    p->pgtbl = proc_pgtbl_init((uint64)p->trapframe);
    p->context.ra = (uint64)proc_return;
    p->context.sp = p->kstack + PAGE_SIZE;
    p->name = "userproc";
    return p;
  }
  return NULL;
}

void proc_free(proc_t *p)
{
  if (p == NULL || !spinlock_holding(&p->lock))
    panic("proc_free without lock");
  mmap_region_t *r = p->mmap;
  while (r != NULL)
  {
    mmap_region_t *next = r->next;
    mmap_region_free(r);
    r = next;
  }
  if (p->pgtbl != NULL)
    uvm_destroy_pgtbl(p->pgtbl);
  if (p->trapframe != NULL)
    pmem_free((uint64)p->trapframe, true);

  uint64 kstack = p->kstack;
  memset((char *)p + sizeof(p->lock), 0,
         sizeof(*p) - sizeof(p->lock));
  p->kstack = kstack;
  p->state = PROC_UNUSED;
}

void proc_make_first(void)
{
  proc_t *p = proc_alloc();
  if (p == NULL)
    panic("cannot allocate first process");
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
  p->name = "proczero";
  proczero = p;
  p->state = PROC_RUNNABLE;
  spinlock_release(&p->lock);
}

void proc_return(void)
{
  static volatile uint32 fs_started;
  proc_t *p = myproc();
  if (p == NULL || !spinlock_holding(&p->lock))
    panic("proc_return without process lock");
  spinlock_release(&p->lock);
  if (__atomic_exchange_n(&fs_started, 1, __ATOMIC_ACQ_REL) == 0)
    fs_init();
  trap_user_return();
}

static void mmap_list_copy(proc_t *dst, const proc_t *src)
{
  mmap_region_t **link = &dst->mmap;
  for (mmap_region_t *r = src->mmap; r != NULL; r = r->next)
  {
    mmap_region_t *copy = mmap_region_alloc();
    copy->begin = r->begin;
    copy->end = r->end;
    copy->perm = r->perm;
    copy->next = NULL;
    *link = copy;
    link = &copy->next;
  }
}

int proc_fork(void)
{
  proc_t *parent = myproc();
  proc_t *child = proc_alloc();
  if (child == NULL)
    return -1;
  child->heap_top = parent->heap_top;
  child->ustack_npage = parent->ustack_npage;
  mmap_list_copy(child, parent);
  if (uvm_copy_pgtbl(parent->pgtbl, child->pgtbl, parent->heap_top,
                     parent->ustack_npage, parent->mmap) < 0)
  {
    proc_free(child);
    spinlock_release(&child->lock);
    return -1;
  }
  memmove(child->trapframe, parent->trapframe, sizeof(*child->trapframe));
  child->trapframe->a0 = 0;
  child->name = parent->name;
  spinlock_acquire(&wait_lock);
  child->parent = parent;
  spinlock_release(&wait_lock);
  int pid = child->pid;
  child->state = PROC_RUNNABLE;
  spinlock_release(&child->lock);
  return pid;
}

void proc_sched(void)
{
  proc_t *p = myproc();
  if (p == NULL || !spinlock_holding(&p->lock))
    panic("proc_sched without process lock");
  if (p->state == PROC_RUNNING)
    panic("proc_sched running process");
  if (intr_get())
    panic("proc_sched with interrupts enabled");
  swtch(&p->context, &mycpu()->scheduler);
}

void proc_yield(void)
{
  proc_t *p = myproc();
  if (p == NULL)
    return;
  spinlock_acquire(&p->lock);
  p->state = PROC_RUNNABLE;
  proc_sched();
  spinlock_release(&p->lock);
}

void proc_sleep(void *sleep_space, spinlock_t *lock)
{
  proc_t *p = myproc();
  if (p == NULL || lock == NULL)
    panic("proc_sleep arguments");
  if (lock != &p->lock)
  {
    spinlock_acquire(&p->lock);
    spinlock_release(lock);
  }
  else if (!spinlock_holding(&p->lock))
    panic("proc_sleep without process lock");
  p->sleep_space = sleep_space;
  p->state = PROC_SLEEPING;
  proc_sched();
  p->sleep_space = NULL;
  if (lock != &p->lock)
  {
    spinlock_release(&p->lock);
    spinlock_acquire(lock);
  }
}

void proc_wakeup(void *sleep_space)
{
  for (uint32 i = 0; i < NPROC; ++i)
  {
    proc_t *p = &proc_list[i];
    spinlock_acquire(&p->lock);
    if (p->state == PROC_SLEEPING && p->sleep_space == sleep_space)
      p->state = PROC_RUNNABLE;
    spinlock_release(&p->lock);
  }
}

void proc_try_wakeup(proc_t *p)
{
  if (p == NULL || !spinlock_holding(&p->lock))
    panic("proc_try_wakeup without lock");
  if (p->state == PROC_SLEEPING)
    p->state = PROC_RUNNABLE;
}

void proc_reparent(proc_t *parent)
{
  for (uint32 i = 0; i < NPROC; ++i)
  {
    proc_t *p = &proc_list[i];
    if (p->parent == parent)
    {
      p->parent = proczero;
      if (proczero != NULL)
      {
        spinlock_acquire(&proczero->lock);
        proc_try_wakeup(proczero);
        spinlock_release(&proczero->lock);
      }
    }
  }
}

void proc_exit(int exit_code)
{
  proc_t *p = myproc();
  if (p == NULL || p == proczero)
    panic("proczero cannot exit");
  spinlock_acquire(&wait_lock);
  proc_reparent(p);
  if (p->parent != NULL)
  {
    spinlock_acquire(&p->parent->lock);
    proc_try_wakeup(p->parent);
    spinlock_release(&p->parent->lock);
  }
  spinlock_acquire(&p->lock);
  p->exit_code = exit_code;
  p->state = PROC_ZOMBIE;
  spinlock_release(&wait_lock);
  proc_sched();
  panic("zombie process returned");
}

int proc_wait(uint64 user_addr)
{
  proc_t *parent = myproc();
  spinlock_acquire(&wait_lock);
  for (;;)
  {
    bool have_child = false;
    for (uint32 i = 0; i < NPROC; ++i)
    {
      proc_t *p = &proc_list[i];
      if (p->parent != parent)
        continue;
      spinlock_acquire(&p->lock);
      have_child = true;
      if (p->state == PROC_ZOMBIE)
      {
        int pid = p->pid;
        if (user_addr != 0 &&
            uvm_copyout(parent->pgtbl, user_addr,
                        (uint64)&p->exit_code, sizeof(p->exit_code)) < 0)
        {
          spinlock_release(&p->lock);
          spinlock_release(&wait_lock);
          return -1;
        }
        proc_free(p);
        spinlock_release(&p->lock);
        spinlock_release(&wait_lock);
        return pid;
      }
      spinlock_release(&p->lock);
    }
    if (!have_child)
    {
      spinlock_release(&wait_lock);
      return -1;
    }
    proc_sleep(parent, &wait_lock);
  }
}

void proc_scheduler(void)
{
  cpu_t *c = mycpu();
  c->proc = NULL;
  for (;;)
  {
    intr_on();
    for (uint32 i = 0; i < NPROC; ++i)
    {
      proc_t *p = &proc_list[i];
      spinlock_acquire(&p->lock);
      if (p->state == PROC_RUNNABLE)
      {
        p->state = PROC_RUNNING;
        c->proc = p;
        swtch(&c->scheduler, &p->context);
        c->proc = NULL;
      }
      spinlock_release(&p->lock);
    }
  }
}
