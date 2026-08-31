#include "sys.h"
int main(void)
{
  helloworld();
  helloworld();

  for (;;)
    asm volatile("nop");
}
