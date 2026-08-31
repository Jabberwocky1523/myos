#include <stdarg.h>

#include "method.h"
#include "../lock/method.h"
#include "../arch/method.h"

static spinlock_t print_lock;
static const char digits[] = "0123456789abcdef";

static void print_uint(uint64 value, uint32 base)
{
  char buf[32];
  int i = 0;

  do {
    buf[i++] = digits[value % base];
    value /= base;
  } while (value != 0);
  while (--i >= 0)
    uart_putc_sync(buf[i]);
}

void printint(int xx, int base, int sign)
{
  uint32 magnitude;
  if (sign && xx < 0) {
    uart_putc_sync('-');
    magnitude = 0U - (uint32)xx;
  } else {
    magnitude = (uint32)xx;
  }
  print_uint(magnitude, (uint32)base);
}

void printptr(uint64 x)
{
  uart_putc_sync('0');
  uart_putc_sync('x');
  for (int shift = 60; shift >= 0; shift -= 4)
    uart_putc_sync(digits[(x >> shift) & 0xf]);
}

void printfloat(double f, int precision)
{
  if (f < 0) {
    uart_putc_sync('-');
    f = -f;
  }
  uint64 whole = (uint64)f;
  print_uint(whole, 10);
  uart_putc_sync('.');
  f -= (double)whole;
  for (int i = 0; i < precision; ++i) {
    f *= 10.0;
    int digit = (int)f;
    uart_putc_sync('0' + digit);
    f -= digit;
  }
}

void print_init(void)
{
  uart_init();
  spinlock_init(&print_lock, "printf");
}

void printf(const char *fmt, ...)
{
  va_list ap;

  spinlock_acquire(&print_lock);
  va_start(ap, fmt);
  for (; *fmt != '\0'; ++fmt) {
    if (*fmt != '%') {
      uart_putc_sync(*fmt);
      continue;
    }

    ++fmt;
    if (*fmt == '\0')
      break;
    switch (*fmt) {
    case 'd':
      printint(va_arg(ap, int), 10, 1);
      break;
    case 'p':
      printint((int)va_arg(ap, unsigned int), 16, 0);
      break;
    case 'x':
      printptr(va_arg(ap, uint64));
      break;
    case 'c':
      uart_putc_sync(va_arg(ap, int));
      break;
    case 's': {
      const char *s = va_arg(ap, const char *);
      if (s == NULL)
        s = "(null)";
      while (*s != '\0')
        uart_putc_sync(*s++);
      break;
    }
    case 'f':
      printfloat(va_arg(ap, double), 6);
      break;
    case '%':
      uart_putc_sync('%');
      break;
    default:
      uart_putc_sync('%');
      uart_putc_sync(*fmt);
      break;
    }
  }
  va_end(ap);
  spinlock_release(&print_lock);
}

void panic(const char *s)
{
  intr_off();
  uart_putc_sync('p'); uart_putc_sync('a'); uart_putc_sync('n');
  uart_putc_sync('i'); uart_putc_sync('c'); uart_putc_sync(':');
  uart_putc_sync(' ');
  if (s != NULL)
    while (*s != '\0')
      uart_putc_sync(*s++);
  uart_putc_sync('\n');
  for (;;)
    asm volatile("wfi");
}

void assert(bool condition, const char *warning)
{
  if (!condition)
    panic(warning);
}
