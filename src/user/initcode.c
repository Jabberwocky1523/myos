
#include "sys.h"

/* Start one Lab9 test in a child and reap it from the permanent init process. */
int main(void)
{
  char path[] = "./test_4";
  char arg0[] = "./test_4";
  char arg1[] = "hello";
  char arg2[] = "world";
  char *argv[] = {arg0, arg1, arg2, 0};
  long pid = syscall(SYS_fork);

  if (pid == 0)
  {
    if (syscall(SYS_exec, path, argv) == -1)
      syscall(SYS_exit, 127);
  }
  else if (pid > 0)
  {
    unsigned int status = 0;
    syscall(SYS_wait, &status);
  }

  for (;;)
    asm volatile("nop");
}
