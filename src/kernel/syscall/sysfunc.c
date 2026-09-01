#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../trap/method.h"

uint64 sys_brk(void)
{
  proc_t *p = myproc();
  uint64 requested;
  arg_uint64(0, &requested);
  if (requested == 0)
    return p->heap_top;

  uint64 result;
  if (requested > p->heap_top)
  {
    uint64 amount = requested - p->heap_top;
    if (amount > 0xffffffffUL)
      return (uint64)-1;
    result = uvm_heap_grow(p->pgtbl, p->heap_top, (uint32)amount);
  }
  else
  {
    uint64 amount = p->heap_top - requested;
    if (amount > 0xffffffffUL)
      return (uint64)-1;
    result = uvm_heap_ungrow(p->pgtbl, p->heap_top, (uint32)amount);
  }
  if (result != (uint64)-1)
    p->heap_top = result;
  return result;
}

uint64 sys_mmap(void)
{
  uint64 start;
  uint32 len;
  arg_uint64(0, &start);
  arg_uint32(1, &len);
  if (len == 0 || (len & PAGE_MASK) != 0)
    return (uint64)-1;
  return uvm_mmap(start, len / PAGE_SIZE, PTE_R | PTE_W);
}

uint64 sys_munmap(void)
{
  uint64 start;
  uint32 len;
  arg_uint64(0, &start);
  arg_uint32(1, &len);
  if (len == 0 || (len & PAGE_MASK) != 0)
    return (uint64)-1;
  return uvm_munmap(start, len / PAGE_SIZE) < 0 ? (uint64)-1 : 0;
}

uint64 sys_print_str(void)
{
  char buffer[256];
  if (arg_str(0, buffer, sizeof(buffer)) < 0)
    return (uint64)-1;
  printf("%s", buffer);
  return 0;
}

uint64 sys_print_int(void)
{
  int value = (int)arg_raw(0);
  printf("%d\n", value);
  return 0;
}

uint64 sys_getpid(void)
{
  return (uint64)myproc()->pid;
}

uint64 sys_fork(void)
{
  int pid = proc_fork();
  return pid < 0 ? (uint64)-1 : (uint64)pid;
}

uint64 sys_wait(void)
{
  uint64 exit_state;
  arg_uint64(0, &exit_state);
  int pid = proc_wait(exit_state);
  return pid < 0 ? (uint64)-1 : (uint64)pid;
}

uint64 sys_exit(void)
{
  proc_exit((int)arg_raw(0));
}

uint64 sys_sleep(void)
{
  uint32 ntick;
  arg_uint32(0, &ntick);
  timer_wait(ntick);
  return 0;
}
