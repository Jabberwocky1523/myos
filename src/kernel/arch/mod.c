#include "method.h"

#define CSR_READ(name) \
  uint64 r_##name(void) { uint64 value; asm volatile("csrr %0, " #name : "=r"(value)); return value; }
#define CSR_WRITE(name) \
  void w_##name(uint64 value) { asm volatile("csrw " #name ", %0" : : "r"(value) : "memory"); }

CSR_READ(mhartid)
CSR_READ(mstatus)
CSR_WRITE(mstatus)
CSR_WRITE(mepc)
CSR_READ(sstatus)
CSR_WRITE(sstatus)
CSR_READ(sip)
CSR_WRITE(sip)
CSR_READ(sie)
CSR_WRITE(sie)
CSR_READ(mie)
CSR_WRITE(mie)
CSR_WRITE(sepc)
CSR_READ(sepc)
CSR_READ(medeleg)
CSR_WRITE(medeleg)
CSR_READ(mideleg)
CSR_WRITE(mideleg)
CSR_WRITE(stvec)
CSR_READ(stvec)
CSR_WRITE(mtvec)
CSR_WRITE(satp)
CSR_READ(satp)
CSR_WRITE(sscratch)
CSR_WRITE(mscratch)
CSR_READ(scause)
CSR_READ(stval)
CSR_WRITE(mcounteren)
CSR_READ(mcounteren)

uint64 r_time(void)
{
  uint64 value;
  asm volatile("rdtime %0" : "=r"(value));
  return value;
}

void intr_on(void) { asm volatile("csrsi sstatus, 2" ::: "memory"); }
void intr_off(void) { asm volatile("csrci sstatus, 2" ::: "memory"); }
int intr_get(void) { return (r_sstatus() & SSTATUS_SIE) != 0; }

uint64 r_sp(void)
{
  uint64 value;
  asm volatile("mv %0, sp" : "=r"(value));
  return value;
}

uint64 r_tp(void)
{
  uint64 value;
  asm volatile("mv %0, tp" : "=r"(value));
  return value;
}

void w_tp(uint64 value)
{
  asm volatile("mv tp, %0" : : "r"(value) : "memory");
}

uint64 r_ra(void)
{
  uint64 value;
  asm volatile("mv %0, ra" : "=r"(value));
  return value;
}

void sfence_vma(void)
{
  asm volatile("sfence.vma zero, zero" ::: "memory");
}

void w_pmpcfg0(uint64 value)
{
  asm volatile("csrw pmpcfg0, %0" : : "r"(value) : "memory");
}

void w_pmpaddr0(uint64 value)
{
  asm volatile("csrw pmpaddr0, %0" : : "r"(value) : "memory");
}

void w_stimecmp(uint64 value)
{
  asm volatile("csrw stimecmp, %0" : : "r"(value) : "memory");
}
