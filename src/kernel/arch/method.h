#ifndef KERNEL_ARCH_METHOD_H
#define KERNEL_ARCH_METHOD_H

#include "type.h"

uint64 r_mhartid(void);
uint64 r_mstatus(void);
void w_mstatus(uint64 value);
void w_mepc(uint64 value);
uint64 r_sstatus(void);
void w_sstatus(uint64 value);
uint64 r_sip(void);
void w_sip(uint64 value);
uint64 r_sie(void);
void w_sie(uint64 value);
uint64 r_mie(void);
void w_mie(uint64 value);
void w_sepc(uint64 value);
uint64 r_sepc(void);
uint64 r_medeleg(void);
void w_medeleg(uint64 value);
uint64 r_mideleg(void);
void w_mideleg(uint64 value);
void w_stvec(uint64 value);
uint64 r_stvec(void);
void w_mtvec(uint64 value);
void w_satp(uint64 value);
uint64 r_satp(void);
void w_sscratch(uint64 value);
void w_mscratch(uint64 value);
uint64 r_scause(void);
uint64 r_stval(void);
void w_mcounteren(uint64 value);
uint64 r_mcounteren(void);
uint64 r_time(void);
void intr_on(void);
void intr_off(void);
int intr_get(void);
uint64 r_sp(void);
uint64 r_tp(void);
void w_tp(uint64 value);
uint64 r_ra(void);
void sfence_vma(void);
void w_pmpcfg0(uint64 value);
void w_pmpaddr0(uint64 value);
void w_stimecmp(uint64 value);

#endif
