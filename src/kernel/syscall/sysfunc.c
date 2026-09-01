#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"

#define SYSCALL_COPY_MAX 256U

uint64 sys_helloworld(void)
{
  proc_t *p = myproc();
  printf("%s hello world\n", p->name);
  return 0;
}

uint64 sys_copyin(void)
{
  proc_t *p = myproc();
  uint64 src;
  uint32 len;
  int values[SYSCALL_COPY_MAX / sizeof(int)];
  arg_uint64(0, &src);
  arg_uint32(1, &len);
  int rc = (len == 0 || len > SYSCALL_COPY_MAX / sizeof(int)) ? -1 : uvm_copyin(p->pgtbl, (uint64)values, src, len * sizeof(int));
  if (rc < 0)
  {
    printf("copyin failed src=%x len=%d\n", src, (int)len);
    return (uint64)-1;
  }
  printf("copyin:");
  for (uint32 i = 0; i < len; ++i)
    printf(" %d", values[i]);
  printf("\n");
  return 0;
}

uint64 sys_copyout(void)
{
  proc_t *p = myproc();
  uint64 dst;
  arg_uint64(0, &dst);
  static const int values[5] = {1, 2, 3, 4, 5};
  int rc = uvm_copyout(p->pgtbl, dst, (uint64)values, sizeof(values));
  if (rc < 0)
  {
    printf("copyout failed dst=%x\n", dst);
    return (uint64)-1;
  }
  return 0;
}

uint64 sys_copyinstr(void)
{
  char buffer[SYSCALL_COPY_MAX];
  int rc = arg_str(0, buffer, sizeof(buffer));
  if (rc < 0)
  {
    printf("copyinstr failed\n");
    return (uint64)-1;
  }
  printf("copyinstr: %s\n", buffer);
  return 0;
}

uint64 sys_brk(void)
{
  proc_t *p = myproc();
  uint64 requested;
  arg_uint64(0, &requested);
  if (requested == 0)
  {
    printf("sys_brk query top=%x\n", p->heap_top);
    return p->heap_top;
  }
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
  if (result == (uint64)-1)
  {
    printf("sys_brk old=%x requested=%x result=-1\n",
           p->heap_top, requested);
    return result;
  }
  printf("sys_brk old=%x requested=%x result=%x\n",
         p->heap_top, requested, result);
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
  uint64 result = uvm_mmap(start, len / PAGE_SIZE, PTE_R | PTE_W);
  printf("sys_mmap start=%x len=%d result=%x\n", start, (int)len, result);
  uvm_show_mmaplist(myproc()->mmap);
  return result;
}

uint64 sys_munmap(void)
{
  uint64 start;
  uint32 len;
  arg_uint64(0, &start);
  arg_uint32(1, &len);
  if (len == 0 || (len & PAGE_MASK) != 0)
    return (uint64)-1;
  int result = uvm_munmap(start, len / PAGE_SIZE);
  printf("sys_munmap start=%x len=%d result=%d\n", start, (int)len, result);
  uvm_show_mmaplist(myproc()->mmap);
  return result < 0 ? (uint64)-1 : 0;
}

uint64 sys_printf(void)
{
  char buffer[256];
  if (arg_str(0, buffer, sizeof(buffer)) < 0)
    return (uint64)-1;
  printf("%s", buffer);
  return 0;
}
