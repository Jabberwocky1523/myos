#ifndef KERNEL_PROC_METHOD_H
#define KERNEL_PROC_METHOD_H

#include "type.h"

extern cpu_t cpus[MAX_HARTS];
extern proc_t proczero;

void proc_init(void);
pgtbl_t proc_pgtbl_init(uint64 trapframe);
void proc_make_first(void) __attribute__((noreturn));
void proc_return(void) __attribute__((noreturn));
cpu_t *mycpu(void);
proc_t *myproc(void);
void swtch(context_t *old, context_t *new);

#endif
