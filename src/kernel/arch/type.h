#ifndef KERNEL_ARCH_TYPE_H
#define KERNEL_ARCH_TYPE_H

#include "../lib/type.h"

#define MAX_HARTS 8

#define MSTATUS_MIE       (1UL << 3)
#define MSTATUS_MPP_MASK  (3UL << 11)
#define MSTATUS_MPP_S     (1UL << 11)
#define MSTATUS_FS_MASK   (3UL << 13)
#define MSTATUS_FS_INITIAL (1UL << 13)

#define MIE_MTIE (1UL << 7)

#define SSTATUS_SIE (1UL << 1)
#define SIE_SSIE     (1UL << 1)
#define SIE_STIE     (1UL << 5)
#define SIE_SEIE     (1UL << 9)
#define SIP_SSIP     (1UL << 1)

#define SCAUSE_INTERRUPT (1UL << 63)
#define SCAUSE_S_SOFTWARE 1UL
#define SCAUSE_S_TIMER    5UL
#define SCAUSE_S_EXTERNAL 9UL

static inline uint64 csr_read_sstatus(void)
{
  uint64 value;
  asm volatile("csrr %0, sstatus" : "=r"(value));
  return value;
}

static inline void csr_write_sstatus(uint64 value)
{
  asm volatile("csrw sstatus, %0" : : "r"(value) : "memory");
}

static inline uint64 csr_read_scause(void)
{
  uint64 value;
  asm volatile("csrr %0, scause" : "=r"(value));
  return value;
}

static inline uint64 csr_read_sepc(void)
{
  uint64 value;
  asm volatile("csrr %0, sepc" : "=r"(value));
  return value;
}

static inline uint64 csr_read_stval(void)
{
  uint64 value;
  asm volatile("csrr %0, stval" : "=r"(value));
  return value;
}

#endif

