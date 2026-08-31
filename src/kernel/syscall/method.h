#ifndef KERNEL_SYSCALL_METHOD_H
#define KERNEL_SYSCALL_METHOD_H

#include "type.h"

void syscall(void);
uint64 arg_raw(int n);
void arg_uint32(int n, uint32 *ip);
void arg_uint64(int n, uint64 *ip);
int arg_str(int n, char *buf, int maxlen);

uint64 sys_helloworld(void);
uint64 sys_copyin(void);
uint64 sys_copyout(void);
uint64 sys_copyinstr(void);
uint64 sys_brk(void);
uint64 sys_mmap(void);
uint64 sys_munmap(void);
uint64 sys_printf(void);

#endif
