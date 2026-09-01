#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
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

  inode_t *ip_1, *ip_2;
  uint32 len, cut_len;

  /* 小批量读写测试 */

  int small_src[10], small_dst[10];
  for (int i = 0; i < 10; i++)
    small_src[i] = i;

  ip_1 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
  inode_lock(ip_1);
  inode_print(ip_1, "small_data");

  printf("writing data...\n\n");
  cut_len = 10 * sizeof(int);
  for (uint32 offset = 0; offset < 400 * cut_len; offset += cut_len)
  {
    len = inode_write_data(ip_1, offset, cut_len, small_src, false);
    assert(len == cut_len, "write fail 1!");
  }
  inode_print(ip_1, "small_data");

  len = inode_read_data(ip_1, 120 * cut_len + 4, cut_len, small_dst, false);
  assert(len == cut_len, "read fail 1!");
  printf("read data:");
  for (int i = 0; i < 10; i++)
    printf(" %d", small_dst[i]);
  printf("\n\n");

  ip_1->disk_info.nlink = 0;
  inode_unlock(ip_1);
  inode_put(ip_1);

  /* 大批量读写测试 */

  char *big_src, big_dst[9];
  uint64 big_pages[5];
  big_dst[8] = 0;

  /* 申请五个连续物理页面 (初始化阶段, 通常来说能拿到连续的) */
  for (uint32 i = 0; i < 5; i++)
  {
    big_pages[i] = pmem_alloc(true);
    if (i != 0)
      assert(big_pages[i] == big_pages[0] - PGSIZE * i,
             "contiguous fail!");
  }
  /* pmem_alloc按地址从高到低返回，连续区的起点是最后分配的页。 */
  big_src = (char *)big_pages[4];

  for (uint32 i = 0; i < 5 * (PGSIZE / 8); i++)
    for (uint32 j = 0; j < 8; j++)
      big_src[i * 8 + j] = 'A' + j;

  ip_2 = inode_create(INODE_TYPE_DATA, INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
  inode_lock(ip_2);
  inode_print(ip_2, "big_data");

  printf("writing data...\n\n");
  cut_len = PGSIZE * 4 + 1110;
  for (uint32 offset = 0; offset < cut_len * 10000; offset += cut_len)
  {
    len = inode_write_data(ip_2, offset, cut_len, big_src, false);
    assert(len == cut_len, "write fail 2!");
  }
  inode_print(ip_2, "big_data");

  len = inode_read_data(ip_2, cut_len * 10000 - 8, 8, big_dst, false);
  assert(len == 8, "read fail 2");
  printf("read data: %s\n", big_dst);

  ip_2->disk_info.nlink = 0;
  inode_unlock(ip_2);
  inode_put(ip_2);

  for (uint32 i = 0; i < 5; i++)
    pmem_free(big_pages[i], true);

  printf("============= test end =============\n");

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
