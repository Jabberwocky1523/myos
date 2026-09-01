#include "sys.h"
#define NUM 20
#define N_BUFFER 8
int main()
{
  unsigned int block_num[NUM];
  unsigned int inode_num[NUM];

  for (int i = 0; i < NUM; i++)
    block_num[i] = syscall(SYS_alloc_block);
  syscall(SYS_flush_buffer, N_BUFFER);
  syscall(SYS_show_bitmap, 0);
  for (int i = 0; i < NUM; i += 2)
    syscall(SYS_free_block, block_num[i]);
  syscall(SYS_flush_buffer, N_BUFFER);
  syscall(SYS_show_bitmap, 0);
  for (int i = 1; i < NUM; i += 2)
    syscall(SYS_free_block, block_num[i]);
  syscall(SYS_flush_buffer, N_BUFFER);
  syscall(SYS_show_bitmap, 0);
  for (int i = 0; i < NUM; i++)
    inode_num[i] = syscall(SYS_alloc_inode);
  syscall(SYS_flush_buffer, N_BUFFER);
  syscall(SYS_show_bitmap, 1);
  for (int i = 0; i < NUM; i++)
    syscall(SYS_free_inode, inode_num[i]);
  syscall(SYS_flush_buffer, N_BUFFER);
  syscall(SYS_show_bitmap, 1);

  while (1)
    ;
}
