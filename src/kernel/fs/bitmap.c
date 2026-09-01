#include "method.h"
#include "../lib/method.h"

int bitmap_search_and_set(uint32 bitmap_block_num, uint32 valid_count)
{
  if (valid_count > BITS_PER_BLOCK)
    return -1;
  buffer_t *b = buffer_get(bitmap_block_num);
  buffer_read(b);
  for (uint32 bit = 0; bit < valid_count; ++bit)
  {
    uint8 mask = (uint8)(1U << (bit & 7U));
    if ((b->data[bit >> 3] & mask) == 0)
    {
      b->data[bit >> 3] |= mask;
      buffer_write(b);
      buffer_put(b);
      return (int)bit;
    }
  }
  buffer_put(b);
  return -1;
}

void bitmap_clear(uint32 bitmap_block_num, uint32 index)
{
  if (index >= BITS_PER_BLOCK)
    panic("bitmap_clear index");
  buffer_t *b = buffer_get(bitmap_block_num);
  buffer_read(b);
  uint8 mask = (uint8)(1U << (index & 7U));
  if ((b->data[index >> 3] & mask) == 0)
  {
    buffer_put(b);
    panic("bitmap double free");
  }
  b->data[index >> 3] &= (uint8)~mask;
  buffer_write(b);
  buffer_put(b);
}

static int bitmap_alloc(uint32 start, uint32 blocks, uint32 count)
{
  uint32 offset = 0;
  for (uint32 i = 0; i < blocks; ++i)
  {
    uint32 valid = count - offset;
    if (valid > BITS_PER_BLOCK)
      valid = BITS_PER_BLOCK;
    int bit = bitmap_search_and_set(start + i, valid);
    if (bit >= 0)
      return (int)(offset + (uint32)bit);
    offset += valid;
  }
  return -1;
}

int bitmap_alloc_block(void)
{
  int index = bitmap_alloc(superblock.data_bitmap_start,
                           superblock.data_bitmap_blocks,
                           superblock.data_blocks);
  return index < 0 ? -1 : (int)(superblock.data_start + (uint32)index);
}

int bitmap_alloc_inode(void)
{
  return bitmap_alloc(superblock.inode_bitmap_start,
                      superblock.inode_bitmap_blocks,
                      superblock.ninodes);
}

int bitmap_free_block(uint32 block_num)
{
  if (block_num < superblock.data_start ||
      block_num >= superblock.data_start + superblock.data_blocks)
    return -1;
  uint32 index = block_num - superblock.data_start;
  bitmap_clear(superblock.data_bitmap_start + index / BITS_PER_BLOCK,
               index % BITS_PER_BLOCK);
  return 0;
}

int bitmap_free_inode(uint32 inode_num)
{
  if (inode_num >= superblock.ninodes)
    return -1;
  bitmap_clear(superblock.inode_bitmap_start +
                   inode_num / BITS_PER_BLOCK,
               inode_num % BITS_PER_BLOCK);
  return 0;
}

void bitmap_print(bool print_inode_bitmap)
{
  uint32 start = print_inode_bitmap ? superblock.inode_bitmap_start :
                                     superblock.data_bitmap_start;
  uint32 blocks = print_inode_bitmap ? superblock.inode_bitmap_blocks :
                                      superblock.data_bitmap_blocks;
  uint32 count = print_inode_bitmap ? superblock.ninodes :
                                     superblock.data_blocks;
  uint32 base = print_inode_bitmap ? 0 : superblock.data_start;
  printf("%s bitmap:", print_inode_bitmap ? "inode" : "data");
  uint32 offset = 0;
  for (uint32 i = 0; i < blocks; ++i)
  {
    buffer_t *b = buffer_get(start + i);
    buffer_read(b);
    uint32 valid = count - offset;
    if (valid > BITS_PER_BLOCK)
      valid = BITS_PER_BLOCK;
    for (uint32 bit = 0; bit < valid; ++bit)
      if ((b->data[bit >> 3] & (1U << (bit & 7U))) != 0)
        printf(" %d", (int)(base + offset + bit));
    buffer_put(b);
    offset += valid;
  }
  printf("\n");
}
