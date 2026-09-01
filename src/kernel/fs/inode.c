#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../proc/method.h"

spinlock_t inode_cache_lock;
inode_t inode_cache[N_INODE_CACHE];

/* Recursively release one direct or indirect index subtree. */
bool __free_data_blocks(uint32 block_num, uint32 level)
{
  if (block_num == 0)
    return true;
  if (level == 0)
  {
    if (bitmap_free_block(block_num) < 0)
      panic("free data block");
    return false;
  }

  bool reached_end = false;
  buffer_t *b = buffer_get(block_num);
  buffer_read(b);
  uint32 *index = (uint32 *)b->data;
  for (uint32 i = 0; i < BLOCK_NUMS_PER_BLOCK; ++i)
  {
    if (__free_data_blocks(index[i], level - 1))
    {
      reached_end = true;
      break;
    }
  }
  buffer_put(b);
  if (bitmap_free_block(block_num) < 0)
    panic("free index block");
  return reached_end;
}

/* Release every data and index block referenced by an inode. */
void free_data_blocks(uint32 *inode_index)
{
  if (inode_index == NULL)
    panic("free_data_blocks null");
  for (uint32 i = 0; i < N_INODE_INDEX; ++i)
  {
    uint32 level = i < N_DIRECT_INDEX ? 0 :
                   (i < N_DIRECT_INDEX + N_INDIRECT_INDEX ? 1 : 2);
    if (inode_index[i] != 0)
      __free_data_blocks(inode_index[i], level);
    inode_index[i] = 0;
  }
}

/* Resolve a logical block and allocate exactly the next missing block. */
int locate_or_add_block(uint32 *inode_index, uint32 logical_block_num)
{
  if (inode_index == NULL || logical_block_num >= MAX_FILE_BLOCKS)
    return -1;

  if (logical_block_num < N_DIRECT_INDEX)
  {
    if (inode_index[logical_block_num] == 0)
    {
      int block_num = bitmap_alloc_block();
      if (block_num < 0)
        return -1;
      inode_index[logical_block_num] = (uint32)block_num;
    }
    return (int)inode_index[logical_block_num];
  }

  logical_block_num -= N_DIRECT_INDEX;
  if (logical_block_num < N_INDIRECT_INDEX * BLOCK_NUMS_PER_BLOCK)
  {
    uint32 root_slot = N_DIRECT_INDEX +
                       logical_block_num / BLOCK_NUMS_PER_BLOCK;
    uint32 index_slot = logical_block_num % BLOCK_NUMS_PER_BLOCK;
    bool new_root = false;
    if (inode_index[root_slot] == 0)
    {
      int root = bitmap_alloc_block();
      if (root < 0)
        return -1;
      inode_index[root_slot] = (uint32)root;
      new_root = true;
    }

    buffer_t *b = buffer_get(inode_index[root_slot]);
    if (new_root)
    {
      memset(b->data, 0, BLOCK_SIZE);
      buffer_write(b);
    }
    else
      buffer_read(b);
    uint32 *index = (uint32 *)b->data;
    if (index[index_slot] == 0)
    {
      int block_num = bitmap_alloc_block();
      if (block_num < 0)
      {
        buffer_put(b);
        if (new_root)
        {
          bitmap_free_block(inode_index[root_slot]);
          inode_index[root_slot] = 0;
        }
        return -1;
      }
      index[index_slot] = (uint32)block_num;
      buffer_write(b);
    }
    int result = (int)index[index_slot];
    buffer_put(b);
    return result;
  }

  logical_block_num -= N_INDIRECT_INDEX * BLOCK_NUMS_PER_BLOCK;
  uint32 first_slot = logical_block_num / BLOCK_NUMS_PER_BLOCK;
  uint32 second_slot = logical_block_num % BLOCK_NUMS_PER_BLOCK;
  uint32 root_slot = N_INODE_INDEX - 1;
  bool new_root = false;
  if (inode_index[root_slot] == 0)
  {
    int root = bitmap_alloc_block();
    if (root < 0)
      return -1;
    inode_index[root_slot] = (uint32)root;
    new_root = true;
  }

  buffer_t *root = buffer_get(inode_index[root_slot]);
  if (new_root)
  {
    memset(root->data, 0, BLOCK_SIZE);
    buffer_write(root);
  }
  else
    buffer_read(root);
  uint32 *first_index = (uint32 *)root->data;
  bool new_branch = false;
  if (first_index[first_slot] == 0)
  {
    int branch = bitmap_alloc_block();
    if (branch < 0)
    {
      buffer_put(root);
      if (new_root)
      {
        bitmap_free_block(inode_index[root_slot]);
        inode_index[root_slot] = 0;
      }
      return -1;
    }
    first_index[first_slot] = (uint32)branch;
    new_branch = true;
    buffer_write(root);
  }
  uint32 branch_block = first_index[first_slot];
  buffer_put(root);

  buffer_t *branch = buffer_get(branch_block);
  if (new_branch)
  {
    memset(branch->data, 0, BLOCK_SIZE);
    buffer_write(branch);
  }
  else
    buffer_read(branch);
  uint32 *second_index = (uint32 *)branch->data;
  if (second_index[second_slot] == 0)
  {
    int data_block = bitmap_alloc_block();
    if (data_block < 0)
    {
      buffer_put(branch);
      if (new_branch)
      {
        bitmap_free_block(branch_block);
        root = buffer_get(inode_index[root_slot]);
        buffer_read(root);
        ((uint32 *)root->data)[first_slot] = 0;
        buffer_write(root);
        buffer_put(root);
      }
      if (new_root)
      {
        bitmap_free_block(inode_index[root_slot]);
        inode_index[root_slot] = 0;
      }
      return -1;
    }
    second_index[second_slot] = (uint32)data_block;
    buffer_write(branch);
  }
  int result = (int)second_index[second_slot];
  buffer_put(branch);
  return result;
}

/* Acquire a reference to an inode cache entry. */
inode_t *inode_get(uint32 inode_num)
{
  if (inode_num >= superblock.ninodes)
    return NULL;
  spinlock_acquire(&inode_cache_lock);
  inode_t *empty = NULL;
  for (uint32 i = 0; i < N_INODE_CACHE; ++i)
  {
    inode_t *ip = &inode_cache[i];
    if (ip->ref != 0 && ip->inode_num == inode_num)
    {
      ip->ref++;
      spinlock_release(&inode_cache_lock);
      return ip;
    }
    if (empty == NULL && ip->ref == 0)
      empty = ip;
  }
  if (empty == NULL)
  {
    spinlock_release(&inode_cache_lock);
    panic("inode cache full");
  }
  empty->inode_num = inode_num;
  empty->ref = 1;
  empty->valid = false;
  memset(&empty->disk_info, 0, sizeof(empty->disk_info));
  spinlock_release(&inode_cache_lock);
  return empty;
}

/* Allocate and initialize an on-disk inode, returning it unlocked. */
inode_t *inode_create(uint16 type, uint16 major, uint16 minor)
{
  int inode_num = bitmap_alloc_inode();
  if (inode_num < 0)
    return NULL;
  inode_disk_t disk_info;
  memset(&disk_info, 0, sizeof(disk_info));
  disk_info.type = type;
  disk_info.major = major;
  disk_info.minor = minor;
  inode_rw((uint32)inode_num, &disk_info, true);
  inode_t *ip = inode_get((uint32)inode_num);
  if (ip == NULL)
  {
    bitmap_free_inode((uint32)inode_num);
    return NULL;
  }
  inode_lock(ip);
  inode_unlock(ip);
  return ip;
}

/* Duplicate an existing inode cache reference. */
inode_t *inode_dup(inode_t *ip)
{
  if (ip == NULL)
    return NULL;
  spinlock_acquire(&inode_cache_lock);
  if (ip->ref == 0)
  {
    spinlock_release(&inode_cache_lock);
    panic("inode_dup ref");
  }
  ip->ref++;
  spinlock_release(&inode_cache_lock);
  return ip;
}

/* Lock an inode and lazily load its on-disk metadata. */
void inode_lock(inode_t *ip)
{
  if (ip == NULL || ip->ref == 0)
    panic("inode_lock arguments");
  sleeplock_acquire(&ip->slk);
  if (!ip->valid)
  {
    inode_rw(ip->inode_num, &ip->disk_info, false);
    if (ip->disk_info.type == INODE_TYPE_NONE)
    {
      sleeplock_release(&ip->slk);
      panic("inode has no type");
    }
    ip->valid = true;
  }
}

/* Release an inode's sleep lock. */
void inode_unlock(inode_t *ip)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk))
    panic("inode_unlock");
  sleeplock_release(&ip->slk);
}

/* Drop a cache reference and delete an unlinked final reference. */
void inode_put(inode_t *ip)
{
  if (ip == NULL)
    return;
  spinlock_acquire(&inode_cache_lock);
  if (ip->ref == 0)
  {
    spinlock_release(&inode_cache_lock);
    panic("inode_put ref");
  }
  if (ip->ref == 1 && ip->valid && ip->disk_info.nlink == 0)
  {
    sleeplock_acquire(&ip->slk);
    spinlock_release(&inode_cache_lock);
    inode_delete(ip);
    sleeplock_release(&ip->slk);
    spinlock_acquire(&inode_cache_lock);
  }
  ip->ref--;
  if (ip->ref == 0)
  {
    ip->valid = false;
    ip->inode_num = INVALID_INODE_NUM;
  }
  spinlock_release(&inode_cache_lock);
}

/* Remove an unlinked inode and all blocks owned by it. */
void inode_delete(inode_t *ip)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.nlink != 0)
    panic("inode_delete state");
  free_data_blocks(ip->disk_info.index);
  memset(&ip->disk_info, 0, sizeof(ip->disk_info));
  inode_rw(ip->inode_num, &ip->disk_info, true);
  if (bitmap_free_inode(ip->inode_num) < 0)
    panic("free inode bitmap");
  ip->valid = false;
}

/* Copy a byte range from a locked inode to kernel or user memory. */
int inode_read_data(inode_t *ip, uint32 offset, uint32 len, void *dst,
                    bool is_user_dst)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk) || dst == NULL)
    panic("inode_read_data state");
  if (offset > ip->disk_info.size)
    return 0;
  if (len > ip->disk_info.size - offset)
    len = ip->disk_info.size - offset;

  uint32 done = 0;
  while (done < len)
  {
    uint32 position = offset + done;
    int block_num = locate_or_add_block(ip->disk_info.index,
                                        position / BLOCK_SIZE);
    if (block_num < 0)
      break;
    buffer_t *b = buffer_get((uint32)block_num);
    buffer_read(b);
    uint32 block_offset = position % BLOCK_SIZE;
    uint32 count = BLOCK_SIZE - block_offset;
    if (count > len - done)
      count = len - done;
    int copied = 0;
    if (is_user_dst)
    {
      proc_t *p = myproc();
      if (p == NULL ||
          uvm_copyout(p->pgtbl, (uint64)dst + done,
                      (uint64)b->data + block_offset, count) < 0)
        copied = -1;
    }
    else
      memmove((uint8 *)dst + done, b->data + block_offset, count);
    buffer_put(b);
    if (copied < 0)
      return done == 0 ? -1 : (int)done;
    done += count;
  }
  return (int)done;
}

/* Copy a byte range into a locked inode and persist its metadata. */
int inode_write_data(inode_t *ip, uint32 offset, uint32 len,
                     const void *src, bool is_user_src)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk) || src == NULL)
    panic("inode_write_data state");
  if (offset > ip->disk_info.size || len > MAX_FILE_SIZE - offset)
    return -1;

  uint32 done = 0;
  while (done < len)
  {
    uint32 position = offset + done;
    int block_num = locate_or_add_block(ip->disk_info.index,
                                        position / BLOCK_SIZE);
    if (block_num < 0)
      break;
    buffer_t *b = buffer_get((uint32)block_num);
    buffer_read(b);
    uint32 block_offset = position % BLOCK_SIZE;
    uint32 count = BLOCK_SIZE - block_offset;
    if (count > len - done)
      count = len - done;
    int copied = 0;
    if (is_user_src)
    {
      proc_t *p = myproc();
      if (p == NULL ||
          uvm_copyin(p->pgtbl, (uint64)b->data + block_offset,
                     (uint64)src + done, count) < 0)
        copied = -1;
    }
    else
      memmove(b->data + block_offset, (const uint8 *)src + done, count);
    if (copied == 0)
      buffer_write(b);
    buffer_put(b);
    if (copied < 0)
      break;
    done += count;
  }
  if (offset + done > ip->disk_info.size)
    ip->disk_info.size = offset + done;
  inode_rw(ip->inode_num, &ip->disk_info, true);
  if (done == 0 && len != 0)
    return -1;
  return (int)done;
}

/* Print a locked inode's metadata and index roots for diagnostics. */
void inode_print(inode_t *ip, char *name)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk))
    panic("inode_print state");
  printf("inode %d %s: type=%d nlink=%d size=%d\n",
         (int)ip->inode_num, name == NULL ? "" : name,
         (int)ip->disk_info.type, (int)ip->disk_info.nlink,
         (int)ip->disk_info.size);
  printf("  index:");
  for (uint32 i = 0; i < N_INODE_INDEX; ++i)
    printf(" %d", (int)ip->disk_info.index[i]);
  printf("\n");
}
