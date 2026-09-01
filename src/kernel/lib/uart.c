#include "method.h"

#define UART0 0x10000000UL
#define RHR 0
#define THR 0
#define IER 1
#define FCR 2
#define LCR 3
#define LSR 5

#define LCR_BAUD_LATCH (1 << 7)
#define LSR_RX_READY   (1 << 0)
#define LSR_TX_IDLE    (1 << 5)

static volatile uint8 *uart_reg(uint64 offset)
{
  return (volatile uint8 *)(UART0 + offset);
}

void uart_init(void)
{
  *uart_reg(IER) = 0x00;
  *uart_reg(LCR) = LCR_BAUD_LATCH;
  *uart_reg(0) = 0x03;
  *uart_reg(1) = 0x00;
  *uart_reg(LCR) = 0x03;
  *uart_reg(FCR) = 0x07;
}

void uart_enable_rx_interrupt(void)
{
  *uart_reg(IER) = 0x01;
}

void uart_putc_sync(int c)
{
  while ((*uart_reg(LSR) & LSR_TX_IDLE) == 0)
    ;
  *uart_reg(THR) = (uint8)c;
}

int uart_getc_sync(void)
{
  if ((*uart_reg(LSR) & LSR_RX_READY) == 0)
    return -1;
  return *uart_reg(RHR);
}

void uart_intr(void)
{
  int c;
  while ((c = uart_getc_sync()) != -1) {
    if (c == '\b' || c == 0x7f) {
      printf("\b \b");
    } else if (c == '\r' || c == '\n') {
      printf("\r\n");
    } else {
      printf("%c", c);
    }
  }
}
