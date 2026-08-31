#ifndef KERNEL_TRAP_METHOD_H
#define KERNEL_TRAP_METHOD_H

#include "type.h"

void kernel_vector(void);
void timer_vector(void);
void trap_kernel_init(void);
void trap_kernel_inithart(void);
void trap_kernel_handler(void);
int interrupt_info(void);
void external_interrupt_handler(void);
void timer_interrupt_handler(void);

void trap_user_handler(void);
void trap_user_return(void) __attribute__((noreturn));

void timer_init(void);
void timer_create(void);
void timer_update(void);
uint64 timer_get_ticks(void);

void plic_init(void);
void plic_inithart(void);
int plic_claim(void);
void plic_complete(int irq);

#endif
