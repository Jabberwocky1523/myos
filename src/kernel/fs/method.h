#ifndef KERNEL_FS_METHOD_H
#define KERNEL_FS_METHOD_H

#include "type.h"

extern superblock_t superblock;
extern spinlock_t inode_cache_lock;
extern inode_t inode_cache[N_INODE_CACHE];

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
bool buffer_valid_pointer(buffer_t *buf);
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
void block_rw(uint32 block_num, void *buf, bool write_it);
void inode_rw(uint32 inode_num, inode_disk_t *ip, bool write_it);

bool __free_data_blocks(uint32 block_num, uint32 level);
void free_data_blocks(uint32 *inode_index);
int locate_or_add_block(uint32 *inode_index, uint32 logical_block_num);
inode_t *inode_get(uint32 inode_num);
inode_t *inode_create(uint16 type, uint16 major, uint16 minor);
inode_t *inode_dup(inode_t *ip);
void inode_lock(inode_t *ip);
void inode_unlock(inode_t *ip);
void inode_put(inode_t *ip);
void inode_delete(inode_t *ip);
int inode_read_data(inode_t *ip, uint32 offset, uint32 len, void *dst,
                    bool is_user_dst);
int inode_write_data(inode_t *ip, uint32 offset, uint32 len,
                     const void *src, bool is_user_src);
void inode_print(inode_t *ip, char *name);

uint32 dentry_search(inode_t *ip, char *name);
int dentry_create(inode_t *ip, uint32 inode_num, char *name);
uint32 dentry_delete(inode_t *ip, char *name);
void dentry_print(inode_t *ip);
char *get_element(char *path, char *name);
inode_t *__path_to_inode(char *path, char *name, bool find_parent_inode);
inode_t *path_to_inode(char *path);
inode_t *path_to_parent_inode(char *path, char *name);

#endif
