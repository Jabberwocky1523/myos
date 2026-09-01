#ifndef KERNEL_FS_TYPE_H
#define KERNEL_FS_TYPE_H

#include "../lib/type.h"
#include "../lock/type.h"

#define BLOCK_SIZE 4096U
#define FS_MAGIC 0x4d594f53U
#define FS_NBLOCKS 1311787U
#define FS_NINODES 65536U
#define BITS_PER_BLOCK (BLOCK_SIZE * 8U)
#define N_BUFFER 8U
#define VIRTIO_NUM 8U

#define FS_INODE_SIZE 64U
#define N_INODE_CACHE 64U
#define ROOT_INODE 0U
#define INVALID_INODE_NUM 0xffffffffU

#define INODE_TYPE_NONE 0U
#define INODE_TYPE_DIRECTORY 1U
#define INODE_TYPE_FILE 2U
#define INODE_TYPE_DEVICE 3U
#define INODE_TYPE_DIR INODE_TYPE_DIRECTORY
#define INODE_TYPE_DATA INODE_TYPE_FILE

/* major和minor的默认取值(代表磁盘设备) */
#define INODE_MAJOR_DEFAULT 1 // 默认的主设备号
#define INODE_MINOR_DEFAULT 1 // 默认的次设备号

#define BIT_PER_BYTE 8
#define BIT_PER_BLOCK (BLOCK_SIZE * BIT_PER_BYTE)

#define N_DIRECT_INDEX 10U
#define N_INDIRECT_INDEX 2U
#define N_INODE_INDEX 13U
#define BLOCK_NUMS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32))
#define MAX_FILE_BLOCKS                                       \
  (N_DIRECT_INDEX + N_INDIRECT_INDEX * BLOCK_NUMS_PER_BLOCK + \
   BLOCK_NUMS_PER_BLOCK * BLOCK_NUMS_PER_BLOCK)
#define MAX_FILE_SIZE (MAX_FILE_BLOCKS * BLOCK_SIZE)

#define DENTRY_NAME_SIZE 60U
#define DENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(dentry_t))

#define BUFFER_BLOCK_NONE 0xffffffffU

/* index字段相关 */
#define INODE_INDEX_1 (10)                            // 直接映射 (10个格子)
#define INODE_INDEX_2 (10 + 2)                        // 一级间接映射 (2个格子)
#define INODE_INDEX_3 (10 + 2 + 1)                    // 二级间接映射 (1个格子)
#define INODE_BLOCK_INDEX_1 (10)                      // 直接映射 (40KB)
#define INODE_BLOCK_INDEX_2 (10 + 2048)               // 一级间接映射 (40KB + 8MB)
#define INODE_BLOCK_INDEX_3 (10 + 2048 + 1024 * 1024) // 二级间接映射 (40KB + 8MB + 4GB)

typedef struct superblock
{
  uint32 magic;
  uint32 block_size;
  uint32 nblocks;
  uint32 ninodes;
  uint32 inode_bitmap_start;
  uint32 inode_bitmap_blocks;
  uint32 inode_start;
  uint32 inode_blocks;
  uint32 data_bitmap_start;
  uint32 data_bitmap_blocks;
  uint32 data_start;
  uint32 data_blocks;
} superblock_t;

typedef struct inode_disk
{
  uint16 type;
  uint16 major;
  uint16 minor;
  uint16 nlink;
  uint32 size;
  uint32 index[N_INODE_INDEX];
} inode_disk_t;

typedef struct inode
{
  uint32 inode_num;
  uint32 ref;
  bool valid;
  sleeplock_t slk;
  inode_disk_t disk_info;
} inode_t;

typedef struct dentry
{
  uint32 inode_num;
  char name[DENTRY_NAME_SIZE];
} dentry_t;

typedef struct buffer buffer_t;

typedef struct buffer_node
{
  struct buffer_node *prev;
  struct buffer_node *next;
  buffer_t *buffer;
} buffer_node_t;

struct buffer
{
  sleeplock_t lock;
  uint32 block_num;
  uint32 ref;
  bool valid;
  volatile bool disk;
  uint8 *data;
  buffer_node_t node;
};

_Static_assert(sizeof(inode_disk_t) == FS_INODE_SIZE,
               "on-disk inode must be 64 bytes");
_Static_assert(sizeof(dentry_t) == 64,
               "on-disk dentry must be 64 bytes");

typedef struct virtq_desc
{
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
} virtq_desc_t;

typedef struct virtq_avail
{
  uint16 flags;
  uint16 idx;
  uint16 ring[VIRTIO_NUM];
  uint16 unused;
} virtq_avail_t;

typedef struct virtq_used_elem
{
  uint32 id;
  uint32 len;
} virtq_used_elem_t;

typedef struct virtq_used
{
  uint16 flags;
  uint16 idx;
  virtq_used_elem_t ring[VIRTIO_NUM];
} virtq_used_t;

typedef struct virtio_blk_req
{
  uint32 type;
  uint32 reserved;
  uint64 sector;
} virtio_blk_req_t;

#endif
