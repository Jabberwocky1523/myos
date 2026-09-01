#include "sys.h"

#define PGSIZE 4096UL
#define VA_MAX (1UL << 38)
#define TRAMPOLINE (VA_MAX - PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
#define MMAP_END (TRAPFRAME - 16UL * 256UL * PGSIZE)
#define MMAP_BEGIN (MMAP_END - 64UL * 256UL * PGSIZE)

static void stop(void) __attribute__((noreturn));
static void fail(const char *message) __attribute__((noreturn));

static void stop(void)
{
  for (;;)
    asm volatile("nop");
}

static void fail(const char *message)
{
  user_printf(message);
  stop();
}

// static void test_copy(void)
// {
//   int values[5];
//   const char *message = "hello, world";
//   if (syscall(SYS_copyout, values) != 0 ||
//       syscall(SYS_copyin, values, 5) != 0 ||
//       syscall(SYS_copyinstr, message) != 0)
//     fail("lab5 copy test failed\n");
//   user_printf("lab5 test 1 passed\n");
// }

// static void test_brk(void)
// {
//   unsigned long heap_top = syscall(SYS_brk, 0);
//   heap_top = syscall(SYS_brk, 0);
//   heap_top = syscall(SYS_brk, heap_top + PGSIZE * 9);
//   heap_top = syscall(SYS_brk, heap_top);
//   heap_top = syscall(SYS_brk, heap_top - PGSIZE * 5);
// }
// static void test_stack()
// {
//   char tmp[PGSIZE * 4];
//   tmp[PGSIZE * 3] = 'h';
//   tmp[PGSIZE * 3 + 1] = 'e';
//   tmp[PGSIZE * 3 + 2] = 'l';
//   tmp[PGSIZE * 3 + 3] = 'l';
//   tmp[PGSIZE * 3 + 4] = 'o';
//   tmp[PGSIZE * 3 + 5] = '\0';
//   if (syscall(SYS_copyinstr, tmp + PGSIZE * 3) != 0)
//     fail("lab5 stack test failed\n");
//   tmp[0] = 'w';
//   tmp[1] = 'o';
//   tmp[2] = 'r';
//   tmp[3] = 'l';
//   tmp[4] = 'd';
//   tmp[5] = '\0';
//   if (syscall(SYS_copyinstr, tmp) != 0)
//     fail("lab5 stack test failed\n");
//   user_printf("lab5 test 2 passed\n");
// }
static void test_mmap(void)
{
  unsigned long a = syscall(SYS_mmap, MMAP_BEGIN + 4 * PGSIZE,
                            3 * PGSIZE);
  syscall(SYS_mmap, MMAP_BEGIN + 10 * PGSIZE, 2 * PGSIZE);
  syscall(SYS_mmap, MMAP_BEGIN + 2 * PGSIZE, 2 * PGSIZE);
  syscall(SYS_mmap, MMAP_BEGIN + 12 * PGSIZE, PGSIZE);
  syscall(SYS_mmap, MMAP_BEGIN + 7 * PGSIZE, 3 * PGSIZE);
  syscall(SYS_mmap, MMAP_BEGIN, 2 * PGSIZE);
  syscall(SYS_mmap, 0, 10 * PGSIZE);
  if ((long)a == -1)
    fail("lab5 mmap test failed\n");
  syscall(SYS_munmap, MMAP_BEGIN + 10 * PGSIZE, 5 * PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN, 10 * PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN + 17 * PGSIZE, 2 * PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN + 15 * PGSIZE, 2 * PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN + 19 * PGSIZE, 2 * PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN + 22 * PGSIZE, PGSIZE);
  syscall(SYS_munmap, MMAP_BEGIN + 21 * PGSIZE, PGSIZE);
  user_printf("lab5 tests 3 and 4 passed\n");
}

int main(void)
{
  // test_copy();
  // test_brk();
  // test_stack();
  test_mmap();

  while (1)
    ;
  return 0;
  stop();
}
