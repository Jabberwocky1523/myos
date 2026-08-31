#ifndef KERNEL_LIB_METHOD_H
#define KERNEL_LIB_METHOD_H

#include "type.h"

void uart_init(void);
void uart_putc_sync(int c);
int uart_getc_sync(void);
void uart_intr(void);

void print_init(void);
void printint(int xx, int base, int sign);
void printptr(uint64 x);
void printfloat(double f, int precision);
void printf(const char *fmt, ...);
void panic(const char *s) __attribute__((noreturn));
void assert(bool condition, const char *warning);

#endif
