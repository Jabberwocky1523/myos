#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../../user/syscall_num.h"

static syscall_fn_t syscall_table[] = {
  [SYS_brk] = sys_brk,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_fork] = sys_fork,
  [SYS_wait] = sys_wait,
  [SYS_exit] = sys_exit,
  [SYS_sleep] = sys_sleep,
  [SYS_getpid] = sys_getpid,
  [SYS_exec] = sys_exec,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_read] = sys_read,
  [SYS_write] = sys_write,
  [SYS_lseek] = sys_lseek,
  [SYS_dup] = sys_dup,
  [SYS_fstat] = sys_fstat,
  [SYS_get_dentries] = sys_get_dentries,
  [SYS_mkdir] = sys_mkdir,
  [SYS_chdir] = sys_chdir,
  [SYS_print_cwd] = sys_print_cwd,
  [SYS_link] = sys_link,
  [SYS_unlink] = sys_unlink,
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
