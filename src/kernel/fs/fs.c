#include "method.h"
#include "../lib/method.h"

superblock_t superblock;

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
      sb_print();
}
