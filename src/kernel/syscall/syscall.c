#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../../user/syscall_num.h"

static syscall_fn_t syscall_table[] = {
  [SYS_brk] = sys_brk,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_print_str] = sys_print_str,
  [SYS_print_int] = sys_print_int,
  [SYS_getpid] = sys_getpid,
  [SYS_fork] = sys_fork,
  [SYS_wait] = sys_wait,
  [SYS_exit] = sys_exit,
  [SYS_sleep] = sys_sleep,
};

uint64 arg_raw(int n)
{
  proc_t *p = myproc();
  if (p == NULL || p->trapframe == NULL)
    panic("syscall argument without process");
  switch (n)
  {
    case 0: return p->trapframe->a0;
    case 1: return p->trapframe->a1;
    case 2: return p->trapframe->a2;
    case 3: return p->trapframe->a3;
    case 4: return p->trapframe->a4;
    case 5: return p->trapframe->a5;
    default: panic("invalid syscall argument");
  }
}

void arg_uint32(int n, uint32 *ip)
{
  if (ip == NULL)
    panic("null uint32 syscall argument");
  *ip = (uint32)arg_raw(n);
}

void arg_uint64(int n, uint64 *ip)
{
  if (ip == NULL)
    panic("null uint64 syscall argument");
  *ip = arg_raw(n);
}

int arg_str(int n, char *buf, int maxlen)
{
  proc_t *p = myproc();
  if (p == NULL || buf == NULL || maxlen <= 0)
    return -1;
  return uvm_copyin_str(p->pgtbl, (uint64)buf, arg_raw(n),
                        (uint32)maxlen);
}

void syscall(void)
{
  proc_t *p = myproc();
  if (p == NULL || p->trapframe == NULL)
    panic("syscall without process");
  uint64 number = p->trapframe->a7;
  uint64 count = sizeof(syscall_table) / sizeof(syscall_table[0]);
  if (number >= count || syscall_table[number] == NULL)
  {
    printf("%s: unknown syscall %d\n", p->name, (int)number);
    p->trapframe->a0 = (uint64)-1;
    return;
  }
  p->trapframe->a0 = syscall_table[number]();
}
