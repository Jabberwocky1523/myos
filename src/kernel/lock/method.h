#ifndef KERNEL_LOCK_METHOD_H
#define KERNEL_LOCK_METHOD_H

#include "type.h"

void push_off(void);
void pop_off(void);
void spinlock_init(spinlock_t *lk, const char *name);
bool spinlock_holding(spinlock_t *lk);
void spinlock_acquire(spinlock_t *lk);
void spinlock_release(spinlock_t *lk);
uint64 hart_id(void);

#endif

