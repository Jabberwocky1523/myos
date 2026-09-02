#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "mkfs.h"

static int disk_fd;              // 磁盘映像的文件描述符
static super_block_t sb;         // 超级块
static int global_inode_num = 0; // 全局的inode_num
static int global_block_num = 0; // 全局的block_num

static char bitmap_buf[BLOCK_SIZE]; // bitmap区域的读写缓冲 供block_alloc和inode_alloc使用
static char inode_buf[BLOCK_SIZE];  // inode区域的读写缓冲 供inode_rw使用
static char data_buf[BLOCK_SIZE];   // data区的读写缓冲 block_rw使用
static char input_buf[BLOCK_SIZE];  // 用户ELF输入缓冲

/*-------------------------关于小端序-------------------------*/

// 转换成小端序
unsigned short xshort(unsigned short x)
{
  unsigned short y;
  unsigned char *a = (unsigned char *)&y;
  a[0] = x;
  a[1] = x >> 8;
  return y;
}

// 转换成小端序
unsigned int xint(unsigned int x)
{
  unsigned int y;
  unsigned char *a = (unsigned char *)&y;
  a[0] = x;
  a[1] = x >> 8;
  a[2] = x >> 16;
  a[3] = x >> 24;
  return y;
}

/*-------------------------磁盘区域读写能力-------------------------*/

/* 读取/写回1个block */
void block_rw(unsigned int block_num, void *buf, bool write_it)
{
  if (lseek(disk_fd, BLOCK_SIZE * block_num, 0) != BLOCK_SIZE * block_num)
  {
    perror("lseek");
    exit(1);
  }

  if (write_it)
  {
    if (write(disk_fd, buf, BLOCK_SIZE) != BLOCK_SIZE)
    {
      perror("write");
      exit(1);
    }
  }
  else
  {
    if (read(disk_fd, buf, BLOCK_SIZE) != BLOCK_SIZE)
    {
      perror("read");
      exit(1);
    }
  }
}

/* 读取/写回1个inode */
void inode_rw(unsigned int inode_num, inode_disk_t *ip, bool write_it)
{
  unsigned int block_num = sb.inode_firstblock + inode_num / INODE_PER_BLOCK;
  unsigned int byte_offset = (inode_num % INODE_PER_BLOCK) * sizeof(inode_disk_t);

  if (write_it)
  {
    block_rw(block_num, inode_buf, false);
    memcpy(inode_buf + byte_offset, ip, sizeof(inode_disk_t));
    block_rw(block_num, inode_buf, true);
  }
  else
  {
    block_rw(block_num, inode_buf, false);
    memcpy(ip, inode_buf + byte_offset, sizeof(inode_disk_t));
  }
}

/* 从磁盘中申请1个空闲的data block */
unsigned int block_alloc()
{
  unsigned int block_num, byte_offset, bit_offset;

  block_num = sb.data_bitmap_firstblock + global_block_num / BIT_PER_BLOCK;
  bit_offset = global_block_num % BIT_PER_BLOCK;
  byte_offset = bit_offset / BIT_PER_BYTE;
  bit_offset = bit_offset % BIT_PER_BYTE;

  block_rw(block_num, bitmap_buf, false);
  bitmap_buf[byte_offset] |= (1 << bit_offset);
  block_rw(block_num, bitmap_buf, true);

  return sb.data_firstblock + global_block_num++;
}

/* 从磁盘中申请1个空闲inode */
unsigned int inode_alloc()
{
  unsigned int block_num, byte_offset, bit_offset;

  block_num = sb.inode_bitmap_firstblock + global_inode_num / BIT_PER_BLOCK;
  bit_offset = global_inode_num % BIT_PER_BLOCK;
  byte_offset = bit_offset / BIT_PER_BYTE;
  bit_offset = bit_offset % BIT_PER_BYTE;

  block_rw(block_num, bitmap_buf, false);
  bitmap_buf[byte_offset] |= (1 << bit_offset);
  block_rw(block_num, bitmap_buf, true);

  return global_inode_num++;
}

/*-------------------------inode精细化管理-------------------------*/

/* inode初始化 */
void inode_init(inode_disk_t *ip, short type, short major, short minor)
{
  ip->type = type;
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  ip->size = 0;
  for (int i = 0; i < 13; i++)
    ip->index[i] = 0;
}

/* 获取或分配一个直接、一级间接或二级间接数据块。 */
unsigned int get_or_alloc_block(inode_disk_t *ip, unsigned int logical_block)
{
  unsigned int index[BLOCK_SIZE / sizeof(unsigned int)];
  unsigned int per_block = BLOCK_SIZE / sizeof(unsigned int);
  if (logical_block < INODE_INDEX_1)
  {
    if (ip->index[logical_block] == 0)
      ip->index[logical_block] = block_alloc();
    return ip->index[logical_block];
  }

  logical_block -= INODE_INDEX_1;
  if (logical_block < 2U * per_block)
  {
    unsigned int root_slot = INODE_INDEX_1 + logical_block / per_block;
    unsigned int slot = logical_block % per_block;
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
    return index[slot];
  }

  logical_block -= 2U * per_block;
  unsigned int first = logical_block / per_block;
  unsigned int second = logical_block % per_block;
  if (first >= per_block)
  {
    fprintf(stderr, "file exceeds inode index capacity\n");
    exit(1);
  }
  if (ip->index[INODE_INDEX_2] == 0)
  {
    ip->index[INODE_INDEX_2] = block_alloc();
    memset(index, 0, sizeof(index));
    block_rw(ip->index[INODE_INDEX_2], index, true);
  }
  else
    block_rw(ip->index[INODE_INDEX_2], index, false);
  unsigned int branch = index[first];
  if (branch == 0)
  {
    branch = block_alloc();
    index[first] = branch;
    block_rw(ip->index[INODE_INDEX_2], index, true);
    memset(index, 0, sizeof(index));
    block_rw(branch, index, true);
  }
  else
    block_rw(branch, index, false);
  if (index[second] == 0)
  {
    index[second] = block_alloc();
    block_rw(branch, index, true);
  }
  return index[second];
}

/* 对inode管理的数据做追加写。 */
void inode_append(inode_disk_t *ip, void *data, unsigned int len)
{
  char *source = data;
  while (len != 0)
  {
    unsigned int block_num = get_or_alloc_block(ip,
                                                 ip->size / BLOCK_SIZE);
    unsigned int offset = ip->size % BLOCK_SIZE;
    unsigned int count = MIN(BLOCK_SIZE - offset, len);
    block_rw(block_num, data_buf, false);
    memcpy(data_buf + offset, source, count);
    block_rw(block_num, data_buf, true);
    ip->size += count;
    source += count;
    len -= count;
  }
}

/* 获取路径的末级文件名。 */
void get_name_from_path(char *path, char *name)
{
  char *last = path;
  for (char *p = path; *p != '\0'; ++p)
    if (*p == '/')
      last = p + 1;
  if (*last == '\0' || strlen(last) >= MAXLEN_FILENAME)
  {
    fprintf(stderr, "invalid image filename: %s\n", path);
    exit(1);
  }
  strcpy(name, last);
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "usage: %s disk.img [user-elf ...]\n", argv[0]);
    return 1;
  }
  assert(BLOCK_SIZE % sizeof(inode_disk_t) == 0);

  /* step-1: 填充 superblock 结构体 */
  sb.magic_num = FS_MAGIC;
  sb.block_size = BLOCK_SIZE;
  sb.inode_bitmap_firstblock = 1;
  sb.inode_bitmap_blocks = COUNT_BLOCKS(N_INODE, BIT_PER_BLOCK);
  sb.inode_firstblock = sb.inode_bitmap_firstblock + sb.inode_bitmap_blocks;
  sb.inode_blocks = COUNT_BLOCKS(N_INODE, INODE_PER_BLOCK);
  sb.data_bitmap_firstblock = sb.inode_firstblock + sb.inode_blocks;
  sb.data_bitmap_blocks = COUNT_BLOCKS(N_DATA_BLOCK, BIT_PER_BLOCK);
  sb.data_firstblock = sb.data_bitmap_firstblock + sb.data_bitmap_blocks;
  sb.data_blocks = N_DATA_BLOCK;
  sb.total_inodes = N_INODE;
  sb.total_blocks = 1 + sb.inode_bitmap_blocks + sb.inode_blocks + sb.data_bitmap_blocks + sb.data_blocks;

  /* step-2: 创建磁盘文件 */
  disk_fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (disk_fd < 0)
  {
    perror(argv[1]);
    exit(1);
  }

  /* step-3: 稀疏扩展为清零磁盘映像 */
  memset(data_buf, 0, BLOCK_SIZE);
  if (ftruncate(disk_fd, (off_t)sb.total_blocks * BLOCK_SIZE) < 0)
  {
    perror("ftruncate");
    return 1;
  }
  block_rw(0, data_buf, true);

  /* step-4: 制作根目录 */
  inode_disk_t root;
  unsigned int root_num = inode_alloc();
  if (root_num != ROOT_INODE_NUM)
  {
    printf("invalid root inode = %u\n", root_num);
    return -1;
  }
  inode_init(&root, INODE_TYPE_DIR, INODE_MAJOR_DEFAULT,
             INODE_MINOR_DEFAULT);
  dentry_t dentry;
  memset(&dentry, 0, sizeof(dentry));
  dentry.inode_num = root_num;
  strcpy(dentry.name, ".");
  inode_append(&root, &dentry, sizeof(dentry));
  strcpy(dentry.name, "..");
  inode_append(&root, &dentry, sizeof(dentry));

  /* step-5: 将当前构建的每个用户ELF加入根目录 */
  for (int argument = 2; argument < argc; ++argument)
  {
    int input_fd = open(argv[argument], O_RDONLY);
    if (input_fd < 0)
    {
      perror(argv[argument]);
      return 1;
    }
    inode_disk_t file;
    unsigned int file_num = inode_alloc();
    inode_init(&file, INODE_TYPE_DATA, INODE_MAJOR_DEFAULT,
               INODE_MINOR_DEFAULT);
    for (;;)
    {
      ssize_t count = read(input_fd, input_buf, sizeof(input_buf));
      if (count < 0)
      {
        perror("read user ELF");
        return 1;
      }
      if (count == 0)
        break;
      inode_append(&file, input_buf, (unsigned int)count);
    }
    close(input_fd);

    inode_disk_t disk_file = file;
    disk_file.type = xshort(disk_file.type);
    disk_file.major = xshort(disk_file.major);
    disk_file.minor = xshort(disk_file.minor);
    disk_file.nlink = xshort(disk_file.nlink);
    disk_file.size = xint(disk_file.size);
    for (int i = 0; i < INODE_INDEX_3; ++i)
      disk_file.index[i] = xint(disk_file.index[i]);
    inode_rw(file_num, &disk_file, true);

    memset(&dentry, 0, sizeof(dentry));
    dentry.inode_num = file_num;
    get_name_from_path(argv[argument], dentry.name);
    inode_append(&root, &dentry, sizeof(dentry));
  }

  /* step-7: 写回super block和inode */
  sb.magic_num = xint(sb.magic_num);
  sb.block_size = xint(sb.block_size);
  sb.inode_bitmap_blocks = xint(sb.inode_bitmap_blocks);
  sb.inode_bitmap_firstblock = xint(sb.inode_bitmap_firstblock);
  sb.inode_blocks = xint(sb.inode_blocks);
  sb.inode_firstblock = xint(sb.inode_firstblock);
  sb.data_bitmap_blocks = xint(sb.data_bitmap_blocks);
  sb.data_bitmap_firstblock = xint(sb.data_bitmap_firstblock);
  sb.data_blocks = xint(sb.data_blocks);
  sb.data_firstblock = xint(sb.data_firstblock);
  sb.total_inodes = xint(sb.total_inodes);
  sb.total_blocks = xint(sb.total_blocks);
  memcpy(data_buf, &sb, sizeof(sb));
  block_rw(0, data_buf, true);

  root.type = xshort(root.type);
  root.major = xshort(root.major);
  root.minor = xshort(root.minor);
  root.nlink = xshort(root.nlink);
  root.size = xint(root.size);
  for (int i = 0; i < INODE_INDEX_3; ++i)
    root.index[i] = xint(root.index[i]);
  inode_rw(root_num, &root, true);

  /* step-8: 关闭磁盘文件 */
  close(disk_fd);

  return 0;
}
