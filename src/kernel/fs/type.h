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

#define BUFFER_BLOCK_NONE 0xffffffffU

typedef struct superblock {
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

typedef struct buffer buffer_t;

typedef struct buffer_node {
  struct buffer_node *prev;
  struct buffer_node *next;
  buffer_t *buffer;
} buffer_node_t;

struct buffer {
  sleeplock_t lock;
  uint32 block_num;
  uint32 ref;
  bool valid;
  volatile bool disk;
  uint8 *data;
  buffer_node_t node;
};

typedef struct virtq_desc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
} virtq_desc_t;

typedef struct virtq_avail {
  uint16 flags;
  uint16 idx;
  uint16 ring[VIRTIO_NUM];
  uint16 unused;
} virtq_avail_t;

typedef struct virtq_used_elem {
  uint32 id;
  uint32 len;
} virtq_used_elem_t;

typedef struct virtq_used {
  uint16 flags;
  uint16 idx;
  virtq_used_elem_t ring[VIRTIO_NUM];
} virtq_used_t;

typedef struct virtio_blk_req {
  uint32 type;
  uint32 reserved;
  uint64 sector;
} virtio_blk_req_t;

#endif
