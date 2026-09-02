#include "../fs/mod.h"
#include "../mem/method.h"
#include "../proc/method.h"
#include "../lock/method.h"
#include "method.h"

#define CTRL(x) ((x) - '@')
#define CONSOLE_BACKSPACE 0x100

static console_t cons;

/* Initialize the line-oriented console input ring. */
void cons_init(void)
{
  spinlock_init(&cons.lock, "console");
  cons.read = 0;
  cons.write = 0;
  cons.edit = 0;
}

/* Emit one console character, including the visual backspace sequence. */
void cons_putc(int c)
{
  if (c == CONSOLE_BACKSPACE)
  {
    uart_putc_sync('\b');
    uart_putc_sync(' ');
    uart_putc_sync('\b');
  }
  else
    uart_putc_sync(c);
}

/* Copy bytes from kernel or user memory to the UART console. */
uint32 cons_write(uint32 len, uint64 src, bool is_user_src)
{
  proc_t *p = myproc();
  for (uint32 i = 0; i < len; ++i)
  {
    char c;
    if (is_user_src)
    {
      if (p == NULL || uvm_copyin(p->pgtbl, (uint64)&c, src + i, 1) < 0)
        return (uint32)-1;
    }
    else
      c = *(char *)(src + i);
    cons_putc(c);
  }
  return len;
}

/* Read one published input line into kernel or user memory. */
uint32 cons_read(uint32 len, uint64 dst, bool is_user_dst)
{
  if (len == 0)
    return 0;
  proc_t *p = myproc();
  uint32 copied = 0;
  spinlock_acquire(&cons.lock);
  while (copied < len)
  {
    while (cons.read == cons.write)
      proc_sleep(&cons.read, &cons.lock);
    char c = cons.buf[cons.read++ % CONSOLE_BUFFER_SIZE];
    if (c == CTRL('D'))
    {
      if (copied != 0)
        cons.read--;
      break;
    }
    if (is_user_dst)
    {
      if (p == NULL || uvm_copyout(p->pgtbl, dst + copied,
                                   (uint64)&c, 1) < 0)
      {
        spinlock_release(&cons.lock);
        return (uint32)-1;
      }
    }
    else
      *(char *)(dst + copied) = c;
    copied++;
    if (c == '\n')
      break;
  }
  spinlock_release(&cons.lock);
  return copied;
}

/* Apply console editing keys and publish complete lines to readers. */
void cons_edit(int c)
{
  spinlock_acquire(&cons.lock);
  if (c == CTRL('U'))
  {
    while (cons.edit != cons.write &&
           cons.buf[(cons.edit - 1) % CONSOLE_BUFFER_SIZE] != '\n')
    {
      cons.edit--;
      cons_putc(CONSOLE_BACKSPACE);
    }
  }
  else if (c == CTRL('H') || c == 0x7f)
  {
    if (cons.edit != cons.write)
    {
      cons.edit--;
      cons_putc(CONSOLE_BACKSPACE);
    }
  }
  else if (c != 0 && cons.edit - cons.read < CONSOLE_BUFFER_SIZE)
  {
    if (c == '\r')
      c = '\n';
    cons_putc(c);
    cons.buf[cons.edit++ % CONSOLE_BUFFER_SIZE] = (char)c;
    if (c == '\n' || c == CTRL('D') ||
        cons.edit - cons.read == CONSOLE_BUFFER_SIZE)
    {
      cons.write = cons.edit;
      proc_wakeup(&cons.read);
    }
  }
  spinlock_release(&cons.lock);
}
