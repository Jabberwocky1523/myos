#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
superblock_t superblock;

static spinlock_t file_table_lock;
static file_t file_table[N_FILE];

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
  file_init();
  device_init();
  sb_print();
}

/* Initialize the global file object pool. */
void file_init(void)
{
  spinlock_init(&file_table_lock, "file table");
  for (uint32 i = 0; i < N_FILE; ++i)
  {
    memset(&file_table[i], 0, sizeof(file_table[i]));
    spinlock_init(&file_table[i].lock, "file");
  }
}

/* Reserve one file object with a single owning reference. */
file_t *file_alloc(void)
{
  spinlock_acquire(&file_table_lock);
  for (uint32 i = 0; i < N_FILE; ++i)
  {
    if (file_table[i].ref == 0)
    {
      file_table[i].ref = 1;
      file_table[i].readable = false;
      file_table[i].writable = false;
      file_table[i].offset = 0;
      file_table[i].inode = NULL;
      spinlock_release(&file_table_lock);
      return &file_table[i];
    }
  }
  spinlock_release(&file_table_lock);
  return NULL;
}

/* Open an existing inode or create a regular file when requested. */
file_t *file_open(char *path, uint32 open_mode)
{
  if (path == NULL || (open_mode & (OPEN_READ | OPEN_WRITE)) == 0 ||
      (open_mode & ~(OPEN_CREATE | OPEN_READ | OPEN_WRITE)) != 0)
    return NULL;
  inode_t *ip = path_to_inode(path);
  if (ip == NULL && (open_mode & OPEN_CREATE) != 0)
    ip = path_create_inode(path, INODE_TYPE_FILE, INODE_MAJOR_DEFAULT,
                           INODE_MINOR_DEFAULT);
  if (ip == NULL)
    return NULL;

  inode_lock(ip);
  bool valid = ip->disk_info.type != INODE_TYPE_NONE;
  if (ip->disk_info.type == INODE_TYPE_DIRECTORY &&
      (open_mode & OPEN_WRITE) != 0)
    valid = false;
  if (ip->disk_info.type == INODE_TYPE_DEVICE &&
      !device_open_check(ip->disk_info.major, open_mode))
    valid = false;
  inode_unlock(ip);
  if (!valid)
  {
    inode_put(ip);
    return NULL;
  }

  file_t *file = file_alloc();
  if (file == NULL)
  {
    inode_put(ip);
    return NULL;
  }
  file->readable = (open_mode & OPEN_READ) != 0;
  file->writable = (open_mode & OPEN_WRITE) != 0;
  file->inode = ip;
  return file;
}

/* Drop a file reference and release its inode after the final close. */
void file_close(file_t *file)
{
  if (file == NULL)
    return;
  spinlock_acquire(&file_table_lock);
  if (file->ref == 0)
  {
    spinlock_release(&file_table_lock);
    panic("file_close ref");
  }
  file->ref--;
  if (file->ref != 0)
  {
    spinlock_release(&file_table_lock);
    return;
  }
  inode_t *ip = file->inode;
  file->inode = NULL;
  file->readable = false;
  file->writable = false;
  file->offset = 0;
  spinlock_release(&file_table_lock);
  inode_put(ip);
}

/* Read data or a device stream and advance the shared file offset. */
int file_read(file_t *file, uint32 len, uint64 dst, bool is_user_dst)
{
  if (file == NULL || !file->readable || file->inode == NULL)
    return -1;
  inode_t *ip = file->inode;
  inode_lock(ip);
  int result;
  if (ip->disk_info.type == INODE_TYPE_FILE)
  {
    result = inode_read_data(ip, file->offset, len, (void *)dst,
                             is_user_dst);
    if (result > 0)
      file->offset += (uint32)result;
  }
  else if (ip->disk_info.type == INODE_TYPE_DEVICE)
    result = (int)device_read_data(ip->disk_info.major, len, dst,
                                   is_user_dst);
  else
    result = -1;
  inode_unlock(ip);
  return result;
}

/* Write data or a device stream and advance the shared file offset. */
int file_write(file_t *file, uint32 len, uint64 src, bool is_user_src)
{
  if (file == NULL || !file->writable || file->inode == NULL)
    return -1;
  inode_t *ip = file->inode;
  inode_lock(ip);
  int result;
  if (ip->disk_info.type == INODE_TYPE_FILE)
  {
    result = inode_write_data(ip, file->offset, len, (void *)src,
                              is_user_src);
    if (result > 0)
      file->offset += (uint32)result;
  }
  else if (ip->disk_info.type == INODE_TYPE_DEVICE)
    result = (int)device_write_data(ip->disk_info.major, len, src,
                                    is_user_src);
  else
    result = -1;
  inode_unlock(ip);
  return result;
}

/* Move a regular file offset with saturating bounds. */
int file_lseek(file_t *file, uint32 lseek_offset, uint32 lseek_flag)
{
  if (file == NULL || file->inode == NULL || lseek_flag > LSEEK_SUB)
    return -1;
  inode_lock(file->inode);
  if (file->inode->disk_info.type != INODE_TYPE_FILE)
  {
    inode_unlock(file->inode);
    return -1;
  }
  uint32 size = file->inode->disk_info.size;
  if (lseek_flag == LSEEK_SET)
    file->offset = lseek_offset > size ? size : lseek_offset;
  else if (lseek_flag == LSEEK_ADD)
    file->offset = lseek_offset > size - file->offset ? size :
                   file->offset + lseek_offset;
  else
    file->offset = lseek_offset > file->offset ? 0 :
                   file->offset - lseek_offset;
  int result = (int)file->offset;
  inode_unlock(file->inode);
  return result;
}

/* Add a reference to a shared file object. */
file_t *file_dup(file_t *file)
{
  if (file == NULL)
    return NULL;
  spinlock_acquire(&file_table_lock);
  if (file->ref == 0)
  {
    spinlock_release(&file_table_lock);
    return NULL;
  }
  file->ref++;
  spinlock_release(&file_table_lock);
  return file;
}

/* Copy stable inode and offset metadata to the current user process. */
int file_get_stat(file_t *file, uint64 user_dst)
{
  if (file == NULL || file->inode == NULL || user_dst == 0)
    return -1;
  file_stat_t stat;
  inode_lock(file->inode);
  if (file->inode->disk_info.type == INODE_TYPE_FILE)
    stat.type = FILE_TYPE_DATA;
  else if (file->inode->disk_info.type == INODE_TYPE_DIRECTORY)
    stat.type = FILE_TYPE_DIR;
  else if (file->inode->disk_info.type == INODE_TYPE_DEVICE)
    stat.type = FILE_TYPE_DEVICE;
  else
  {
    inode_unlock(file->inode);
    return -1;
  }
  stat.nlink = file->inode->disk_info.nlink;
  stat.size = file->inode->disk_info.size;
  stat.inode_num = file->inode->inode_num;
  stat.offset = file->offset;
  inode_unlock(file->inode);
  proc_t *p = myproc();
  return p == NULL ? -1 : uvm_copyout(p->pgtbl, user_dst,
                                       (uint64)&stat, sizeof(stat));
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
