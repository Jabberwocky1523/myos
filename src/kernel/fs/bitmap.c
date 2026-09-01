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

void bitmap_print(bool print_data_bitmap)
{
  uint32 first_block, bitmap_blocks, total_bits;
  uint32 global_base, current_bit = 0;
  superblock_t sb = superblock;
  if (print_data_bitmap)
  {
    printf("data bitmap alloced bits:\n");
    first_block = sb.data_bitmap_start;
    bitmap_blocks = sb.data_bitmap_blocks;
    total_bits = sb.data_blocks;
    global_base = sb.data_start;
  }
  else
  {
    printf("inode bitmap alloced bits:\n");
    first_block = sb.inode_bitmap_start;
    bitmap_blocks = sb.inode_bitmap_blocks;
    total_bits = sb.ninodes;
    global_base = 0;
  }

  for (uint32 block = 0; block < bitmap_blocks; block++)
  {
    uint32 bitmap_block_num = first_block + block;
    uint32 bits_in_this_block = BIT_PER_BLOCK;

    // 最后一个 block 可能不满
    if (current_bit + BIT_PER_BLOCK > total_bits)
      bits_in_this_block = total_bits - current_bit;

    buffer_t *buf = buffer_get(bitmap_block_num);
    buffer_read(buf);

    // 遍历该 block 中的有效 bit
    for (uint32 byte = 0; byte < bits_in_this_block / BIT_PER_BYTE; byte++)
    {
      for (uint32 shift = 0; shift < BIT_PER_BYTE; shift++)
      {
        if (current_bit >= total_bits)
          break;

        uint8 mask = (uint8)(1U << shift);
        if (buf->data[byte] & mask)
          printf("%d ", global_base + current_bit);
        current_bit++;
      }
    }
    buffer_put(buf);
  }
  printf("over!\n\n");
}
