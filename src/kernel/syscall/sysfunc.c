#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../trap/method.h"
#include "../fs/method.h"
#include "../lock/method.h"

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

uint64 sys_alloc_block(void)
{
  int block = bitmap_alloc_block();
  return block < 0 ? (uint64)-1 : (uint64)block;
}

uint64 sys_free_block(void)
{
  uint32 block;
  arg_uint32(0, &block);
  return bitmap_free_block(block) < 0 ? (uint64)-1 : 0;
}

uint64 sys_alloc_inode(void)
{
  int inode = bitmap_alloc_inode();
  return inode < 0 ? (uint64)-1 : (uint64)inode;
}

uint64 sys_free_inode(void)
{
  uint32 inode;
  arg_uint32(0, &inode);
  return bitmap_free_inode(inode) < 0 ? (uint64)-1 : 0;
}

uint64 sys_show_bitmap(void)
{
  uint32 choice;
  arg_uint32(0, &choice);
  if (choice > 1)
    return (uint64)-1;
  bitmap_print(choice == 1);
  return 0;
}

static buffer_t *arg_buffer(int n)
{
  buffer_t *b = (buffer_t *)arg_raw(n);
  if (!buffer_valid_pointer(b) || !sleeplock_holding(&b->lock))
    return NULL;
  return b;
}

uint64 sys_get_block(void)
{
  uint32 block;
  arg_uint32(0, &block);
  if (block >= superblock.nblocks)
    return (uint64)-1;
  return (uint64)buffer_get(block);
}

uint64 sys_read_block(void)
{
  proc_t *p = myproc();
  buffer_t *b = arg_buffer(0);
  uint64 dst;
  arg_uint64(1, &dst);
  if (b == NULL)
    return (uint64)-1;
  buffer_read(b);
  if (uvm_copyout(p->pgtbl, dst, (uint64)b->data, BLOCK_SIZE) == 0)
    return 0;
  if (dst >= USER_STACK_BOTTOM && dst < USER_STACK_TOP &&
      BLOCK_SIZE <= USER_STACK_TOP - dst)
  {
    int64 pages = uvm_ustack_grow(p->pgtbl, p->ustack_npage, dst);
    if (pages >= 0)
    {
      p->ustack_npage = (uint64)pages;
      if (uvm_copyout(p->pgtbl, dst, (uint64)b->data, BLOCK_SIZE) == 0)
        return 0;
    }
  }
  return (uint64)-1;
}

uint64 sys_write_block(void)
{
  proc_t *p = myproc();
  buffer_t *b = arg_buffer(0);
  uint64 src;
  arg_uint64(1, &src);
  if (b == NULL ||
      uvm_copyin(p->pgtbl, (uint64)b->data, src, BLOCK_SIZE) < 0)
    return (uint64)-1;
  buffer_write(b);
  return 0;
}

uint64 sys_put_block(void)
{
  buffer_t *b = arg_buffer(0);
  if (b == NULL)
    return (uint64)-1;
  buffer_put(b);
  return 0;
}

uint64 sys_show_buffer(void)
{
  buffer_print();
  return 0;
}

uint64 sys_flush_buffer(void)
{
  uint32 count;
  arg_uint32(0, &count);
  buffer_freemem(count);
  return 0;
}
