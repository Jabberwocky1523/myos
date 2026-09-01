#ifndef KERNEL_LOCK_TYPE_H
#define KERNEL_LOCK_TYPE_H

#include "../lib/type.h"

typedef struct spinlock {
  volatile uint32 locked;
  const char *name;
  uint64 owner;
} spinlock_t;

typedef struct sleeplock {
  spinlock_t lock;
  bool locked;
  int pid;
  const char *name;
} sleeplock_t;

#endif
