#ifndef MYOS_MKFS_H
#define MYOS_MKFS_H

#include <stdint.h>

#define BLOCK_SIZE 4096U
#define FS_MAGIC 0x4d594f53U
#define FS_NBLOCKS 1311787U
#define FS_NINODES 65536U
#define FS_INODE_SIZE 64U

typedef struct superblock {
  uint32_t magic;
  uint32_t block_size;
  uint32_t nblocks;
  uint32_t ninodes;
  uint32_t inode_bitmap_start;
  uint32_t inode_bitmap_blocks;
  uint32_t inode_start;
  uint32_t inode_blocks;
  uint32_t data_bitmap_start;
  uint32_t data_bitmap_blocks;
  uint32_t data_start;
  uint32_t data_blocks;
} superblock_t;

#endif
