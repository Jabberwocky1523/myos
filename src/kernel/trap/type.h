#ifndef KERNEL_TRAP_TYPE_H
#define KERNEL_TRAP_TYPE_H

#include "../lib/type.h"

#define UART0_IRQ 10

typedef struct kernel_trapframe {
  uint64 ra;
  uint64 gp;
  uint64 tp;
  uint64 t0;
  uint64 t1;
  uint64 t2;
  uint64 s0;
  uint64 s1;
  uint64 a0;
  uint64 a1;
  uint64 a2;
  uint64 a3;
  uint64 a4;
  uint64 a5;
  uint64 a6;
  uint64 a7;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
  uint64 t3;
  uint64 t4;
  uint64 t5;
  uint64 t6;
  uint64 sepc;
  uint64 sstatus;
} kernel_trapframe_t;

_Static_assert(sizeof(kernel_trapframe_t) == 256,
               "kernel trap frame must match trap.S");
_Static_assert(__builtin_offsetof(kernel_trapframe_t, a0) == 64,
               "kernel trap frame a0 offset mismatch");
_Static_assert(__builtin_offsetof(kernel_trapframe_t, sepc) == 240,
               "kernel trap frame sepc offset mismatch");
_Static_assert(__builtin_offsetof(kernel_trapframe_t, sstatus) == 248,
               "kernel trap frame sstatus offset mismatch");

#endif
