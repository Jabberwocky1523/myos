#ifndef MYOS_MKFS_H
#define MYOS_MKFS_H

#include <stdint.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096U
#define FS_MAGIC 0x4d594f53U
#define FS_NBLOCKS 1311787U
#define FS_NINODES 65536U
#define FS_INODE_SIZE 64U
#define N_DIRECT_INDEX 10U
#define N_INDIRECT_INDEX 2U
#define N_INODE_INDEX 13U
#define BLOCK_NUMS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define INODE_TYPE_NONE 0U
#define INODE_TYPE_DIRECTORY 1U
#define INODE_TYPE_FILE 2U
#define DENTRY_NAME_SIZE 60U
#define INVALID_INODE_NUM 0xffffffffU

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

typedef struct inode_disk {
  uint16_t type;
  uint16_t major;
  uint16_t minor;
  uint16_t nlink;
  uint32_t size;
  uint32_t index[N_INODE_INDEX];
} inode_disk_t;

typedef struct dentry {
  uint32_t inode_num;
  char name[DENTRY_NAME_SIZE];
} dentry_t;

_Static_assert(sizeof(inode_disk_t) == FS_INODE_SIZE,
               "on-disk inode must be 64 bytes");
_Static_assert(sizeof(dentry_t) == 64,
               "on-disk dentry must be 64 bytes");

unsigned short xshort(unsigned short x);
unsigned int xint(unsigned int x);
void block_rw(unsigned int block_num, void *buf, bool write_it);
void inode_rw(unsigned int inode_num, inode_disk_t *ip, bool write_it);
unsigned int block_alloc(void);
unsigned int inode_alloc(void);
void inode_init(inode_disk_t *ip, short type, short major, short minor);
void inode_append(inode_disk_t *ip, void *data, unsigned int len);

#endif
