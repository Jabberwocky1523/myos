#ifndef KERNEL_FS_METHOD_H
#define KERNEL_FS_METHOD_H

#include "type.h"

extern superblock_t superblock;

void virtio_disk_init(void);
int alloc_desc(void);
void free_desc(int i);
void free_chain(int i);
int alloc3_desc(int idx[3]);
void virtio_disk_rw(buffer_t *b, bool write);
void virtio_disk_intr(void);

void insert_node(buffer_node_t *node, bool insert_active,
                 bool insert_next);
void buffer_init(void);
buffer_t *buffer_get(uint32 block_num);
void buffer_read(buffer_t *buf);
void buffer_write(buffer_t *buf);
void buffer_put(buffer_t *buf);
uint32 buffer_freemem(uint32 buffer_count);
void buffer_print(void);

int bitmap_search_and_set(uint32 bitmap_block_num, uint32 valid_count);
void bitmap_clear(uint32 bitmap_block_num, uint32 index);
int bitmap_alloc_block(void);
int bitmap_alloc_inode(void);
int bitmap_free_block(uint32 block_num);
int bitmap_free_inode(uint32 inode_num);
void bitmap_print(bool print_inode_bitmap);

void sb_print(void);
void fs_init(void);

#endif
