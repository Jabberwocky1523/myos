#include "mkfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
  perror(message);
  exit(1);
}

static void block_write(FILE *image, uint32_t block_num, const void *data)
{
  if (fseek(image, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0)
    fail("mkfs seek");
  if (fwrite(data, BLOCK_SIZE, 1, image) != 1)
    fail("mkfs write");
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s disk.img\n", argv[0]);
    return 1;
  }
  FILE *image = fopen(argv[1], "wb+");
  if (image == NULL)
    fail("mkfs open");

  /* Create a sparse image of the requested size. */
  uint64_t image_size = (uint64_t)FS_NBLOCKS * BLOCK_SIZE;
  if (image_size == 0 || fseek(image, (long)(image_size - 1), SEEK_SET) != 0 ||
      fputc(0, image) == EOF)
    fail("mkfs size");

  superblock_t sb;
  memset(&sb, 0, sizeof(sb));
  sb.magic = FS_MAGIC;
  sb.block_size = BLOCK_SIZE;
  sb.nblocks = FS_NBLOCKS;
  sb.ninodes = FS_NINODES;
  sb.inode_bitmap_start = 1;
  sb.inode_bitmap_blocks = 2;
  sb.inode_start = sb.inode_bitmap_start + sb.inode_bitmap_blocks;
  sb.inode_blocks = 1024;
  sb.data_bitmap_start = 1027;
  sb.data_bitmap_blocks = 40;
  sb.data_start = sb.data_bitmap_start + sb.data_bitmap_blocks;
  sb.data_blocks = FS_NBLOCKS - sb.data_start;

  uint8_t block[BLOCK_SIZE];
  memset(block, 0, sizeof(block));
  memcpy(block, &sb, sizeof(sb));
  block_write(image, 0, block);
  if (fclose(image) != 0)
    fail("mkfs close");

  printf("mkfs: %u blocks, data starts at %u (%u blocks)\n",
         sb.nblocks, sb.data_start, sb.data_blocks);
  return 0;
}
