#ifndef KERNEL_PROC_METHOD_H
#define KERNEL_PROC_METHOD_H

#include "type.h"

extern cpu_t cpus[MAX_HARTS];
extern proc_t proc_list[NPROC];
extern proc_t *proczero;

void proc_init(void);
proc_t *proc_alloc(void);
void proc_free(proc_t *p);
pgtbl_t proc_pgtbl_init(uint64 trapframe);
void proc_make_first(void);
void proc_return(void) __attribute__((noreturn));
int proc_fork(void);
void proc_yield(void);
void proc_reparent(proc_t *parent);
void proc_try_wakeup(proc_t *p);
void proc_exit(int exit_code) __attribute__((noreturn));
int proc_wait(uint64 user_addr);
void proc_sleep(void *sleep_space, spinlock_t *lock);
void proc_wakeup(void *sleep_space);
void proc_sched(void);
void proc_scheduler(void) __attribute__((noreturn));
cpu_t *mycpu(void);
proc_t *myproc(void);
void swtch(context_t *old, context_t *new);

void load_segment(inode_t *ip, pgtbl_t pgtbl, uint64 seg_start,
                  uint64 va_start, uint32 len);
uint64 prepare_heap(pgtbl_t new_pgtbl, inode_t *ip, elf_header_t *eh);
uint64 prepare_stack(pgtbl_t new_pgtbl, char **argv, int *arg_count);
int proc_exec(char *path, char **argv);

#endif
