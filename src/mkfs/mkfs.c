#include "mkfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *fs_image;
static superblock_t superblock;

/* Report a host I/O failure and terminate image construction. */
static void fail(const char *message)
{
  perror(message);
  exit(1);
}

/* Write a complete block during early image initialization. */
static void block_write(FILE *image, uint32_t block_num, const void *data)
{
  if (fseek(image, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0)
    fail("mkfs seek");
  if (fwrite(data, BLOCK_SIZE, 1, image) != 1)
    fail("mkfs write");
}

/* Encode a 16-bit value in the filesystem's little-endian format. */
unsigned short xshort(unsigned short x)
{
  unsigned short value;
  unsigned char *bytes = (unsigned char *)&value;
  bytes[0] = (unsigned char)x;
  bytes[1] = (unsigned char)(x >> 8);
  return value;
}

/* Encode a 32-bit value in the filesystem's little-endian format. */
unsigned int xint(unsigned int x)
{
  unsigned int value;
  unsigned char *bytes = (unsigned char *)&value;
  bytes[0] = (unsigned char)x;
  bytes[1] = (unsigned char)(x >> 8);
  bytes[2] = (unsigned char)(x >> 16);
  bytes[3] = (unsigned char)(x >> 24);
  return value;
}

/* Read or write one complete filesystem block. */
void block_rw(unsigned int block_num, void *buf, bool write_it)
{
  if (block_num >= superblock.nblocks ||
      fseek(fs_image, (long)block_num * BLOCK_SIZE, SEEK_SET) != 0)
    fail("mkfs block seek");
  if (write_it)
  {
    if (fwrite(buf, BLOCK_SIZE, 1, fs_image) != 1)
      fail("mkfs block write");
    if (fflush(fs_image) != 0)
      fail("mkfs flush");
  }
  else if (fread(buf, BLOCK_SIZE, 1, fs_image) != 1)
    fail("mkfs block read");
}

/* Read or write one inode table slot. */
void inode_rw(unsigned int inode_num, inode_disk_t *ip, bool write_it)
{
  if (inode_num >= superblock.ninodes || ip == NULL)
    fail("mkfs inode arguments");
  uint32_t byte_offset = inode_num * sizeof(*ip);
  uint32_t block_num = superblock.inode_start + byte_offset / BLOCK_SIZE;
  uint32_t block_offset = byte_offset % BLOCK_SIZE;
  uint8_t block[BLOCK_SIZE];
  block_rw(block_num, block, false);
  if (write_it)
  {
    memcpy(block + block_offset, ip, sizeof(*ip));
    block_rw(block_num, block, true);
  }
  else
    memcpy(ip, block + block_offset, sizeof(*ip));
}

/* Allocate the first free data block and persist its bitmap bit. */
unsigned int block_alloc(void)
{
  uint8_t bitmap[BLOCK_SIZE];
  uint32_t offset = 0;
  for (uint32_t block = 0; block < superblock.data_bitmap_blocks; ++block)
  {
    block_rw(superblock.data_bitmap_start + block, bitmap, false);
    uint32_t valid = superblock.data_blocks - offset;
    if (valid > BLOCK_SIZE * 8U)
      valid = BLOCK_SIZE * 8U;
    for (uint32_t bit = 0; bit < valid; ++bit)
    {
      uint8_t mask = (uint8_t)(1U << (bit & 7U));
      if ((bitmap[bit >> 3] & mask) == 0)
      {
        bitmap[bit >> 3] |= mask;
        block_rw(superblock.data_bitmap_start + block, bitmap, true);
        return superblock.data_start + offset + bit;
      }
    }
    offset += valid;
  }
  fail("mkfs no data blocks");
  return 0;
}

/* Allocate the first free inode number and persist its bitmap bit. */
unsigned int inode_alloc(void)
{
  uint8_t bitmap[BLOCK_SIZE];
  uint32_t offset = 0;
  for (uint32_t block = 0; block < superblock.inode_bitmap_blocks; ++block)
  {
    block_rw(superblock.inode_bitmap_start + block, bitmap, false);
    uint32_t valid = superblock.ninodes - offset;
    if (valid > BLOCK_SIZE * 8U)
      valid = BLOCK_SIZE * 8U;
    for (uint32_t bit = 0; bit < valid; ++bit)
    {
      uint8_t mask = (uint8_t)(1U << (bit & 7U));
      if ((bitmap[bit >> 3] & mask) == 0)
      {
        bitmap[bit >> 3] |= mask;
        block_rw(superblock.inode_bitmap_start + block, bitmap, true);
        return offset + bit;
      }
    }
    offset += valid;
  }
  fail("mkfs no inodes");
  return INVALID_INODE_NUM;
}

/* Initialize an empty on-disk inode with its identity fields. */
void inode_init(inode_disk_t *ip, short type, short major, short minor)
{
  memset(ip, 0, sizeof(*ip));
  ip->type = (uint16_t)type;
  ip->major = (uint16_t)major;
  ip->minor = (uint16_t)minor;
}

/* Append bytes while growing direct, single, and double index levels. */
void inode_append(inode_disk_t *ip, void *data, unsigned int len)
{
  uint8_t *source = data;
  uint32_t done = 0;
  while (done < len)
  {
    uint32_t logical = ip->size / BLOCK_SIZE;
    uint32_t block_num;
    if (logical < N_DIRECT_INDEX)
    {
      if (ip->index[logical] == 0)
        ip->index[logical] = block_alloc();
      block_num = ip->index[logical];
    }
    else if (logical < N_DIRECT_INDEX +
                       N_INDIRECT_INDEX * BLOCK_NUMS_PER_BLOCK)
    {
      uint32_t relative = logical - N_DIRECT_INDEX;
      uint32_t root_slot = N_DIRECT_INDEX +
                           relative / BLOCK_NUMS_PER_BLOCK;
      uint32_t slot = relative % BLOCK_NUMS_PER_BLOCK;
      uint32_t index[BLOCK_NUMS_PER_BLOCK];
      if (ip->index[root_slot] == 0)
      {
        ip->index[root_slot] = block_alloc();
        memset(index, 0, sizeof(index));
        block_rw(ip->index[root_slot], index, true);
      }
      else
        block_rw(ip->index[root_slot], index, false);
      if (index[slot] == 0)
      {
        index[slot] = block_alloc();
        block_rw(ip->index[root_slot], index, true);
      }
      block_num = index[slot];
    }
    else
    {
      uint32_t relative = logical - N_DIRECT_INDEX -
                          N_INDIRECT_INDEX * BLOCK_NUMS_PER_BLOCK;
      if (relative >= BLOCK_NUMS_PER_BLOCK * BLOCK_NUMS_PER_BLOCK)
        fail("mkfs file too large");
      uint32_t first_slot = relative / BLOCK_NUMS_PER_BLOCK;
      uint32_t second_slot = relative % BLOCK_NUMS_PER_BLOCK;
      uint32_t first[BLOCK_NUMS_PER_BLOCK];
      uint32_t second[BLOCK_NUMS_PER_BLOCK];
      uint32_t root_slot = N_INODE_INDEX - 1;
      if (ip->index[root_slot] == 0)
      {
        ip->index[root_slot] = block_alloc();
        memset(first, 0, sizeof(first));
        block_rw(ip->index[root_slot], first, true);
      }
      else
        block_rw(ip->index[root_slot], first, false);
      if (first[first_slot] == 0)
      {
        first[first_slot] = block_alloc();
        memset(second, 0, sizeof(second));
        block_rw(first[first_slot], second, true);
        block_rw(ip->index[root_slot], first, true);
      }
      else
        block_rw(first[first_slot], second, false);
      if (second[second_slot] == 0)
      {
        second[second_slot] = block_alloc();
        block_rw(first[first_slot], second, true);
      }
      block_num = second[second_slot];
    }

    uint8_t block[BLOCK_SIZE];
    uint32_t block_offset = ip->size % BLOCK_SIZE;
    if (block_offset == 0)
      memset(block, 0, sizeof(block));
    else
      block_rw(block_num, block, false);
    uint32_t count = BLOCK_SIZE - block_offset;
    if (count > len - done)
      count = len - done;
    memcpy(block + block_offset, source + done, count);
    block_rw(block_num, block, true);
    ip->size += count;
    done += count;
  }
}

/* Construct the sparse disk image and its initial root namespace. */
int main(int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s disk.img\n", argv[0]);
    return 1;
  }
  fs_image = fopen(argv[1], "wb+");
  if (fs_image == NULL)
    fail("mkfs open");

  uint64_t image_size = (uint64_t)FS_NBLOCKS * BLOCK_SIZE;
  if (image_size == 0 ||
      fseek(fs_image, (long)(image_size - 1), SEEK_SET) != 0 ||
      fputc(0, fs_image) == EOF || fflush(fs_image) != 0)
    fail("mkfs size");

  memset(&superblock, 0, sizeof(superblock));
  superblock.magic = FS_MAGIC;
  superblock.block_size = BLOCK_SIZE;
  superblock.nblocks = FS_NBLOCKS;
  superblock.ninodes = FS_NINODES;
  superblock.inode_bitmap_start = 1;
  superblock.inode_bitmap_blocks = 2;
  superblock.inode_start = 3;
  superblock.inode_blocks = 1024;
  superblock.data_bitmap_start = 1027;
  superblock.data_bitmap_blocks = 40;
  superblock.data_start = 1067;
  superblock.data_blocks = FS_NBLOCKS - superblock.data_start;

  uint8_t block[BLOCK_SIZE];
  memset(block, 0, sizeof(block));
  memcpy(block, &superblock, sizeof(superblock));
  block_write(fs_image, 0, block);
  if (fflush(fs_image) != 0)
    fail("mkfs superblock flush");

  uint32_t root_num = inode_alloc();
  uint32_t file_num = inode_alloc();
  if (root_num != 0 || file_num != 1)
    fail("mkfs inode order");

  inode_disk_t root;
  inode_disk_t file;
  inode_init(&root, INODE_TYPE_DIRECTORY, 0, 0);
  inode_init(&file, INODE_TYPE_FILE, 0, 0);
  root.nlink = 2;
  file.nlink = 1;

  dentry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.inode_num = root_num;
  memcpy(entry.name, ".", 2);
  inode_append(&root, &entry, sizeof(entry));
  memset(&entry, 0, sizeof(entry));
  entry.inode_num = root_num;
  memcpy(entry.name, "..", 3);
  inode_append(&root, &entry, sizeof(entry));
  memset(&entry, 0, sizeof(entry));
  entry.inode_num = file_num;
  memcpy(entry.name, "file.txt", 9);
  inode_append(&root, &entry, sizeof(entry));

  char file_context[] = "This is file context";
  inode_append(&file, file_context, sizeof(file_context) - 1U);
  inode_rw(root_num, &root, true);
  inode_rw(file_num, &file, true);

  if (fclose(fs_image) != 0)
    fail("mkfs close");
  printf("mkfs: %u blocks, data starts at %u (%u blocks), root inode %u\n",
         superblock.nblocks, superblock.data_start, superblock.data_blocks,
         root_num);
  return 0;
}
