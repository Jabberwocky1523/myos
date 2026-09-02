#include "method.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../trap/method.h"
#include "../fs/method.h"
#include "../lock/method.h"

uint64 sys_brk(void)
{
  proc_t *p = myproc();
  uint64 requested;
  arg_uint64(0, &requested);
  if (requested == 0)
    return p->heap_top;

  uint64 result;
  if (requested > p->heap_top)
  {
    uint64 amount = requested - p->heap_top;
    if (amount > 0xffffffffUL)
      return (uint64)-1;
    result = uvm_heap_grow(p->pgtbl, p->heap_top, (uint32)amount,
                           PTE_R | PTE_W);
  }
  else
  {
    uint64 amount = p->heap_top - requested;
    if (amount > 0xffffffffUL)
      return (uint64)-1;
    result = uvm_heap_ungrow(p->pgtbl, p->heap_top, (uint32)amount);
  }
  if (result != (uint64)-1)
    p->heap_top = result;
  return result;
}

uint64 sys_mmap(void)
{
  uint64 start;
  uint32 len;
  arg_uint64(0, &start);
  arg_uint32(1, &len);
  if (len == 0 || (len & PAGE_MASK) != 0)
    return (uint64)-1;
  return uvm_mmap(start, len / PAGE_SIZE, PTE_R | PTE_W);
}

uint64 sys_munmap(void)
{
  uint64 start;
  uint32 len;
  arg_uint64(0, &start);
  arg_uint32(1, &len);
  if (len == 0 || (len & PAGE_MASK) != 0)
    return (uint64)-1;
  return uvm_munmap(start, len / PAGE_SIZE) < 0 ? (uint64)-1 : 0;
}

uint64 sys_print_str(void)
{
  char buffer[256];
  if (arg_str(0, buffer, sizeof(buffer)) < 0)
    return (uint64)-1;
  printf("%s", buffer);
  return 0;
}

uint64 sys_print_int(void)
{
  int value = (int)arg_raw(0);
  printf("%d\n", value);
  return 0;
}

uint64 sys_getpid(void)
{
  return (uint64)myproc()->pid;
}

uint64 sys_fork(void)
{
  int pid = proc_fork();
  return pid < 0 ? (uint64)-1 : (uint64)pid;
}

uint64 sys_wait(void)
{
  uint64 exit_state;
  arg_uint64(0, &exit_state);
  int pid = proc_wait(exit_state);
  return pid < 0 ? (uint64)-1 : (uint64)pid;
}

uint64 sys_exit(void)
{
  proc_exit((int)arg_raw(0));
}

uint64 sys_sleep(void)
{
  uint32 ntick;
  arg_uint32(0, &ntick);
  timer_wait(ntick);
  return 0;
}

/* Copy a bounded user argv vector and execute the requested ELF file. */
uint64 sys_exec(void)
{
  proc_t *p = myproc();
  char path[128];
  if (p == NULL || arg_str(0, path, sizeof(path)) < 0)
    return (uint64)-1;
  uint64 user_argv;
  arg_uint64(1, &user_argv);
  uint64 strings_page = pmem_try_alloc(true);
  if (strings_page == 0)
    return (uint64)-1;
  char *argv[ELF_MAXARGS + 1];
  uint32 argc = 0;
  for (; argc < ELF_MAXARGS; ++argc)
  {
    uint64 user_string;
    if (uvm_copyin(p->pgtbl, (uint64)&user_string,
                   user_argv + argc * sizeof(uint64),
                   sizeof(user_string)) < 0)
      goto fail_exec;
    if (user_string == 0)
      break;
    argv[argc] = (char *)(strings_page + argc * ELF_MAXARG_LEN);
    if (uvm_copyin_str(p->pgtbl, (uint64)argv[argc], user_string,
                       ELF_MAXARG_LEN) < 0)
      goto fail_exec;
  }
  if (argc == ELF_MAXARGS)
  {
    uint64 terminator;
    if (uvm_copyin(p->pgtbl, (uint64)&terminator,
                   user_argv + argc * sizeof(uint64),
                   sizeof(terminator)) < 0 || terminator != 0)
      goto fail_exec;
  }
  argv[argc] = NULL;
  int result = proc_exec(path, argv);
  pmem_free(strings_page, true);
  return result < 0 ? (uint64)-1 : (uint64)result;

fail_exec:
  pmem_free(strings_page, true);
  return (uint64)-1;
}

/* Install a file reference in the first free descriptor slot. */
int alloc_fd(file_t *file)
{
  proc_t *p = myproc();
  if (p == NULL || file == NULL)
    return -1;
  for (uint32 fd = 0; fd < N_OPEN_FILE; ++fd)
  {
    if (p->open_file[fd] == NULL)
    {
      p->open_file[fd] = file;
      return (int)fd;
    }
  }
  return -1;
}

/* Open a user path and allocate a process-local descriptor. */
uint64 sys_open(void)
{
  char path[128];
  uint32 mode;
  if (arg_str(0, path, sizeof(path)) < 0)
    return (uint64)-1;
  arg_uint32(1, &mode);
  file_t *file = file_open(path, mode);
  if (file == NULL)
    return (uint64)-1;
  int fd = alloc_fd(file);
  if (fd < 0)
    file_close(file);
  return fd < 0 ? (uint64)-1 : (uint64)fd;
}

/* Close one process-local descriptor. */
uint64 sys_close(void)
{
  uint32 fd;
  arg_uint32(0, &fd);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  file_t *file = p->open_file[fd];
  p->open_file[fd] = NULL;
  file_close(file);
  return 0;
}

/* Read bytes through a validated process descriptor. */
uint64 sys_read(void)
{
  uint32 fd, len;
  uint64 dst;
  arg_uint32(0, &fd);
  arg_uint32(1, &len);
  arg_uint64(2, &dst);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  int result = file_read(p->open_file[fd], len, dst, true);
  return result < 0 ? (uint64)-1 : (uint64)result;
}

/* Write bytes through a validated process descriptor. */
uint64 sys_write(void)
{
  uint32 fd, len;
  uint64 src;
  arg_uint32(0, &fd);
  arg_uint32(1, &len);
  arg_uint64(2, &src);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  int result = file_write(p->open_file[fd], len, src, true);
  return result < 0 ? (uint64)-1 : (uint64)result;
}

/* Move a regular-file descriptor offset. */
uint64 sys_lseek(void)
{
  uint32 fd, offset, flag;
  arg_uint32(0, &fd);
  arg_uint32(1, &offset);
  arg_uint32(2, &flag);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  int result = file_lseek(p->open_file[fd], offset, flag);
  return result < 0 ? (uint64)-1 : (uint64)result;
}

/* Duplicate a descriptor while sharing the underlying file offset. */
uint64 sys_dup(void)
{
  uint32 fd;
  arg_uint32(0, &fd);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  file_t *file = file_dup(p->open_file[fd]);
  int new_fd = alloc_fd(file);
  if (new_fd < 0)
    file_close(file);
  return new_fd < 0 ? (uint64)-1 : (uint64)new_fd;
}

/* Copy descriptor metadata to a user file_stat structure. */
uint64 sys_fstat(void)
{
  uint32 fd;
  uint64 dst;
  arg_uint32(0, &fd);
  arg_uint64(1, &dst);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL)
    return (uint64)-1;
  return file_get_stat(p->open_file[fd], dst) < 0 ? (uint64)-1 : 0;
}

/* Copy all live entries from a directory descriptor. */
uint64 sys_get_dentries(void)
{
  uint32 fd, len;
  uint64 dst;
  arg_uint32(0, &fd);
  arg_uint64(1, &dst);
  arg_uint32(2, &len);
  proc_t *p = myproc();
  if (p == NULL || fd >= N_OPEN_FILE || p->open_file[fd] == NULL ||
      p->open_file[fd]->inode == NULL)
    return (uint64)-1;
  inode_t *ip = p->open_file[fd]->inode;
  inode_lock(ip);
  int result = dentry_transmit(ip, dst, len, true);
  inode_unlock(ip);
  return result < 0 ? (uint64)-1 : (uint64)result;
}

/* Create a directory inode and its dot links. */
uint64 sys_mkdir(void)
{
  char path[128];
  if (arg_str(0, path, sizeof(path)) < 0)
    return (uint64)-1;
  inode_t *ip = path_create_inode(path, INODE_TYPE_DIRECTORY,
                                  INODE_MAJOR_DEFAULT,
                                  INODE_MINOR_DEFAULT);
  if (ip == NULL)
    return (uint64)-1;
  inode_put(ip);
  return 0;
}

/* Replace the current process working-directory reference. */
uint64 sys_chdir(void)
{
  char path[128];
  if (arg_str(0, path, sizeof(path)) < 0)
    return (uint64)-1;
  inode_t *next = path_to_inode(path);
  if (next == NULL)
    return (uint64)-1;
  inode_lock(next);
  bool is_dir = next->disk_info.type == INODE_TYPE_DIRECTORY;
  inode_unlock(next);
  if (!is_dir)
  {
    inode_put(next);
    return (uint64)-1;
  }
  proc_t *p = myproc();
  inode_t *old = p->cwd;
  p->cwd = next;
  inode_put(old);
  return 0;
}

/* Print the current working directory as an absolute path. */
uint64 sys_print_cwd(void)
{
  proc_t *p = myproc();
  char path[128];
  int offset = p == NULL ? -1 : inode_to_path(p->cwd, path, sizeof(path));
  if (offset < 0)
    return (uint64)-1;
  printf("%s\n", path + offset);
  return 0;
}

/* Create a hard link between two user paths. */
uint64 sys_link(void)
{
  char old_path[128], new_path[128];
  if (arg_str(0, old_path, sizeof(old_path)) < 0 ||
      arg_str(1, new_path, sizeof(new_path)) < 0)
    return (uint64)-1;
  return path_link(old_path, new_path) < 0 ? (uint64)-1 : 0;
}

/* Unlink a file or empty directory path. */
uint64 sys_unlink(void)
{
  char path[128];
  if (arg_str(0, path, sizeof(path)) < 0)
    return (uint64)-1;
  return path_unlink(path) < 0 ? (uint64)-1 : 0;
}

uint64 sys_alloc_block(void)
{
  int block = bitmap_alloc_block();
  return block < 0 ? (uint64)-1 : (uint64)block;
}

uint64 sys_free_block(void)
{
  uint32 block;
  arg_uint32(0, &block);
  return bitmap_free_block(block) < 0 ? (uint64)-1 : 0;
}

uint64 sys_alloc_inode(void)
{
  int inode = bitmap_alloc_inode();
  return inode < 0 ? (uint64)-1 : (uint64)inode;
}

uint64 sys_free_inode(void)
{
  uint32 inode;
  arg_uint32(0, &inode);
  return bitmap_free_inode(inode) < 0 ? (uint64)-1 : 0;
}

uint64 sys_show_bitmap(void)
{
  uint32 choice;
  arg_uint32(0, &choice);
  if (choice > 1)
    return (uint64)-1;
  bitmap_print(choice == 1);
  return 0;
}

static buffer_t *arg_buffer(int n)
{
  buffer_t *b = (buffer_t *)arg_raw(n);
  if (!sleeplock_holding(&b->lock))
    return NULL;
  return b;
}

uint64 sys_get_block(void)
{
  uint32 block;
  arg_uint32(0, &block);
  if (block >= superblock.nblocks)
    return (uint64)-1;
  return (uint64)buffer_get(block);
}

uint64 sys_read_block(void)
{
  proc_t *p = myproc();
  buffer_t *b = arg_buffer(0);
  uint64 dst;
  arg_uint64(1, &dst);
  if (b == NULL)
    return (uint64)-1;
  buffer_read(b);
  if (uvm_copyout(p->pgtbl, dst, (uint64)b->data, BLOCK_SIZE) == 0)
    return 0;
  if (dst >= USER_STACK_BOTTOM && dst < USER_STACK_TOP &&
      BLOCK_SIZE <= USER_STACK_TOP - dst)
  {
    int64 pages = uvm_ustack_grow(p->pgtbl, p->ustack_npage, dst);
    if (pages >= 0)
    {
      p->ustack_npage = (uint64)pages;
      if (uvm_copyout(p->pgtbl, dst, (uint64)b->data, BLOCK_SIZE) == 0)
        return 0;
    }
  }
  return (uint64)-1;
}

uint64 sys_write_block(void)
{
  proc_t *p = myproc();
  buffer_t *b = arg_buffer(0);
  uint64 src;
  arg_uint64(1, &src);
  if (b == NULL ||
      uvm_copyin(p->pgtbl, (uint64)b->data, src, BLOCK_SIZE) < 0)
    return (uint64)-1;
  buffer_write(b);
  return 0;
}

uint64 sys_put_block(void)
{
  buffer_t *b = arg_buffer(0);
  if (b == NULL)
    return (uint64)-1;
  buffer_put(b);
  return 0;
}

uint64 sys_show_buffer(void)
{
  buffer_print();
  return 0;
}

uint64 sys_flush_buffer(void)
{
  uint32 count;
  arg_uint32(0, &count);
  buffer_freemem(count);
  return 0;
}
