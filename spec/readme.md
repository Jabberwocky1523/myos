# LAB-6: 单进程走向多进程——进程调度与生命周期

## 测试用例

以下测试只需要修改**user/initcode.c**
**测试1** 

```c
#include "sys.h"

int main()
{
	int pid = syscall(SYS_getpid);
	if (pid == 1) {
		syscall(SYS_print_str, "\nproczero: hello ");
		syscall(SYS_print_str, "world!\n");
	}
	while (1);	
}
```

**测试2** 

请在内核代码合适的位置增加提示性输出

```c
#include "sys.h"

int main()
{
	syscall(SYS_print_str, "level-1!\n");
	syscall(SYS_fork);
	syscall(SYS_print_str, "level-2!\n");
	syscall(SYS_fork);
	syscall(SYS_print_str, "level-3!\n");
	while(1);
}
```

**测试3** 

```c
#include "sys.h"

#define PGSIZE 4096
#define VA_MAX (1ul << 38)
#define MMAP_END (VA_MAX - (2 + 16 * 256) * PGSIZE)
#define MMAP_BEGIN (MMAP_END - 64 * 256 * PGSIZE)

int main()
{
	int pid, i;
	char *str1, *str2, *str3 = "STACK_REGION\n\n";
	char *tmp1 = "MMAP_REGION\n", *tmp2 = "HEAP_REGION\n";
	
	str1 = (char*)syscall(SYS_mmap, MMAP_BEGIN, PGSIZE);
	for (i = 0; tmp1[i] != '\0'; i++)
		str1[i] = tmp1[i];
	str1[i] = '\0';	

	str2 = (char*)syscall(SYS_brk, 0);
	syscall(SYS_brk, (long long int)str2 + PGSIZE);
	for (i = 0; tmp2[i] != '\0'; i++)
		str2[i] = tmp2[i];
	str2[i] = '\0';	

	syscall(SYS_print_str, "\n--------test begin--------\n");
	pid = syscall(SYS_fork);

	if (pid == 0) { // 子进程
		syscall(SYS_print_str, "child proc: hello!\n");
		syscall(SYS_print_str, str1);
		syscall(SYS_print_str, str2);
		syscall(SYS_print_str, str3);
		syscall(SYS_exit, 1234);
	} else { // 父进程
		int exit_state = 0;
		syscall(SYS_wait, &exit_state);
		syscall(SYS_print_str, "parent proc: hello!\n");
		syscall(SYS_print_int, pid);
		if (exit_state == 1234)
			syscall(SYS_print_str, "good boy!\n");
		else
			syscall(SYS_print_str, "bad boy!\n"); 
	}

	syscall(SYS_print_str, "--------test end----------\n");

	while (1);
	
	return 0;
}
```

**测试4** 

请在内核代码合适的位置增加提示性输出

```c
#include "sys.h"

int main()
{
	int pid = syscall(SYS_fork);
	if (pid == 0) {
		syscall(SYS_print_str, "Ready to sleep!\n");
		syscall(SYS_sleep, 30);
		syscall(SYS_print_str, "Ready to exit!\n");
		syscall(SYS_exit, 0);
	} else {
		syscall(SYS_wait, 0);
		syscall(SYS_print_str, "Child exit!\n");
	}
	while(1);
}
```