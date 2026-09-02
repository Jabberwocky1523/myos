#ifndef KERNEL_SYSCALL_METHOD_H
#define KERNEL_SYSCALL_METHOD_H

#include "type.h"
#include "../fs/type.h"

void syscall(void);
uint64 arg_raw(int n);
void arg_uint32(int n, uint32 *ip);
void arg_uint64(int n, uint64 *ip);
int arg_str(int n, char *buf, int maxlen);

uint64 sys_brk(void);
uint64 sys_mmap(void);
uint64 sys_munmap(void);
uint64 sys_print_str(void);
uint64 sys_print_int(void);
uint64 sys_getpid(void);
uint64 sys_fork(void);
uint64 sys_wait(void);
uint64 sys_exit(void);
uint64 sys_sleep(void);
uint64 sys_exec(void);
int alloc_fd(file_t *file);
uint64 sys_open(void);
uint64 sys_close(void);
uint64 sys_read(void);
uint64 sys_write(void);
uint64 sys_lseek(void);
uint64 sys_dup(void);
uint64 sys_fstat(void);
uint64 sys_get_dentries(void);
uint64 sys_mkdir(void);
uint64 sys_chdir(void);
uint64 sys_print_cwd(void);
uint64 sys_link(void);
uint64 sys_unlink(void);
uint64 sys_alloc_block(void);
uint64 sys_free_block(void);
uint64 sys_alloc_inode(void);
uint64 sys_free_inode(void);
uint64 sys_show_bitmap(void);
uint64 sys_get_block(void);
uint64 sys_read_block(void);
uint64 sys_write_block(void);
uint64 sys_put_block(void);
uint64 sys_show_buffer(void);
uint64 sys_flush_buffer(void);

#endif
