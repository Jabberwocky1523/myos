#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"

superblock_t superblock;

/* Print the validated disk geometry. */
void sb_print(void)
{
  printf("superblock:\n");
  printf("  magic=%x block_size=%d blocks=%d inodes=%d\n",
         (uint64)superblock.magic, (int)superblock.block_size,
         (int)superblock.nblocks, (int)superblock.ninodes);
  printf("  inode_bitmap=[%d,%d) inode=[%d,%d)\n",
         (int)superblock.inode_bitmap_start,
         (int)(superblock.inode_bitmap_start +
               superblock.inode_bitmap_blocks),
         (int)superblock.inode_start,
         (int)(superblock.inode_start + superblock.inode_blocks));
  printf("  data_bitmap=[%d,%d) data=[%d,%d)\n",
         (int)superblock.data_bitmap_start,
         (int)(superblock.data_bitmap_start +
               superblock.data_bitmap_blocks),
         (int)superblock.data_start,
         (int)(superblock.data_start + superblock.data_blocks));
}

/* Initialize block and inode caches after validating the superblock. */
void fs_init(void)
{
  buffer_init();
  buffer_t *b = buffer_get(0);
  buffer_read(b);
  memmove(&superblock, b->data, sizeof(superblock));
  buffer_put(b);
  if (superblock.magic != FS_MAGIC ||
      superblock.block_size != BLOCK_SIZE ||
      superblock.nblocks != FS_NBLOCKS ||
      superblock.ninodes != FS_NINODES ||
      superblock.inode_bitmap_start != 1 ||
      superblock.inode_bitmap_blocks != 2 ||
      superblock.inode_start != 3 ||
      superblock.inode_blocks != 1024 ||
      superblock.data_bitmap_start != 1027 ||
      superblock.data_bitmap_blocks != 40 ||
      superblock.data_start != 1067 ||
      superblock.data_start + superblock.data_blocks !=
          superblock.nblocks)
    panic("invalid superblock");
  spinlock_init(&inode_cache_lock, "inode cache");
  for (uint32 i = 0; i < N_INODE_CACHE; ++i)
  {
    memset(&inode_cache[i], 0, sizeof(inode_cache[i]));
    sleeplock_init(&inode_cache[i].slk, "inode");
    inode_cache[i].inode_num = INVALID_INODE_NUM;
  }
  sb_print();

  printf("============= test begin =============\n\n");
  inode_t *rooti, *ip_1, *ip_2;

  rooti = inode_get(ROOT_INODE);
  inode_lock(rooti);
  inode_print(rooti, "root");
  inode_unlock(rooti);

  /* 第一次查看bitmap */
  bitmap_print(false);

  ip_1 = inode_create(INODE_TYPE_DIR, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
  ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
  inode_lock(ip_1);
  inode_lock(ip_2);
  inode_dup(ip_2);

  inode_print(ip_1, "dir");
  inode_print(ip_2, "data");

  /* 第二次查看bitmap */
  bitmap_print(false);

  ip_1->disk_info.nlink = 0;
  ip_2->disk_info.nlink = 0;
  inode_unlock(ip_1);
  inode_unlock(ip_2);
  inode_put(ip_1);
  inode_put(ip_2);

  /* 第三次查看bitmap */
  bitmap_print(false);

  inode_put(ip_2);

  /* 第四次查看bitmap */
  bitmap_print(false);

  printf("============= test end =============\n\n");

  while (1)
    ;
}

/* Transfer one whole block between disk and a kernel buffer. */
void block_rw(uint32 block_num, void *data, bool write_it)
{
  buffer_t *b = buffer_get(block_num);
  if (write_it)
  {
    memmove(b->data, data, BLOCK_SIZE);
    buffer_write(b);
  }
  else
  {
    buffer_read(b);
    memmove(data, b->data, BLOCK_SIZE);
  }
  buffer_put(b);
}

/* Transfer one fixed-size inode between its table slot and memory. */
void inode_rw(uint32 inode_num, inode_disk_t *ip, bool write_it)
{
  if (inode_num >= superblock.ninodes || ip == NULL)
    panic("inode_rw arguments");
  uint32 byte_offset = inode_num * sizeof(inode_disk_t);
  uint32 block_num = superblock.inode_start + byte_offset / BLOCK_SIZE;
  uint32 block_offset = byte_offset % BLOCK_SIZE;
  buffer_t *b = buffer_get(block_num);
  buffer_read(b);
  if (write_it)
  {
    memmove(b->data + block_offset, ip, sizeof(*ip));
    buffer_write(b);
  }
  else
    memmove(ip, b->data + block_offset, sizeof(*ip));
  buffer_put(b);
}
