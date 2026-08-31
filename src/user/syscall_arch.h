#ifndef USER_SYSCALL_ARCH_H
#define USER_SYSCALL_ARCH_H

static inline long __syscall0(long n)
{
  register long a0 asm("a0");
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "=r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline long __syscall1(long n, long a)
{
  register long a0 asm("a0") = a;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
  return a0;
}

static inline long __syscall2(long n, long a, long b)
{
  register long a0 asm("a0") = a;
  register long a1 asm("a1") = b;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
  return a0;
}

static inline long __syscall3(long n, long a, long b, long c)
{
  register long a0 asm("a0") = a;
  register long a1 asm("a1") = b;
  register long a2 asm("a2") = c;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
  return a0;
}

static inline long __syscall4(long n, long a, long b, long c, long d)
{
  register long a0 asm("a0") = a;
  register long a1 asm("a1") = b;
  register long a2 asm("a2") = c;
  register long a3 asm("a3") = d;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
  return a0;
}

static inline long __syscall5(long n, long a, long b, long c, long d, long e)
{
  register long a0 asm("a0") = a;
  register long a1 asm("a1") = b;
  register long a2 asm("a2") = c;
  register long a3 asm("a3") = d;
  register long a4 asm("a4") = e;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
  return a0;
}

static inline long __syscall6(long n, long a, long b, long c,
                              long d, long e, long f)
{
  register long a0 asm("a0") = a;
  register long a1 asm("a1") = b;
  register long a2 asm("a2") = c;
  register long a3 asm("a3") = d;
  register long a4 asm("a4") = e;
  register long a5 asm("a5") = f;
  register long a7 asm("a7") = n;
  asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3),
               "r"(a4), "r"(a5), "r"(a7) : "memory");
  return a0;
}

#endif
