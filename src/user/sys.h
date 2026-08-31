#ifndef USER_SYS_H
#define USER_SYS_H

#include "syscall_arch.h"
#include "syscall_num.h"

static inline long helloworld(void)
{
  return __syscall0(SYS_helloworld);
}

#endif
