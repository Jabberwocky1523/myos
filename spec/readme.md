# LAB-5: 系统调用流程建立 + 用户态虚拟内存管理


## 测试1：用户态和内核态的数据迁移

测试逻辑: 

- 用户读取内核中的数组 (1 2 3 4 5)

- 用户将读到的数组传递给内核, 内核收到后打印出来

- 用户将自己的字符串传递给内核, 内核收到后打印出来

```c
// in initcode.c
#include "sys.h"

int main()
{
    int L[5];
    char* s = "hello, world"; 
    syscall(SYS_copyout, L);
    syscall(SYS_copyin, L, 5);
    syscall(SYS_copyinstr, s);
    while(1);
    return 0;
}
```


## 测试2：堆的手动管理与栈的自动管理

**堆的管理**

```c
// in initcode.c
#include "sys.h"

#define PGSIZE 4096

int main()
{
    long long heap_top = 0;
    
    heap_top = syscall(SYS_brk, 0);
    heap_top = syscall(SYS_brk, heap_top + PGSIZE * 9);
    heap_top = syscall(SYS_brk, heap_top);
    heap_top = syscall(SYS_brk, heap_top - PGSIZE * 5);

    while(1);
    return 0;
}
```

你需要在`sys_brk`中增加一些调试性输出



**栈的管理**

函数内定义非static的长数组就能让栈的大小超过4KB

你也可以通过深度函数递归来实现类似的效果 (比如汉诺塔问题)

```c
// in initcode.c
#include "sys.h"

#define PGSIZE 4096

int main()
{
    char tmp[PGSIZE * 4];

    tmp[PGSIZE * 3] = 'h';
    tmp[PGSIZE * 3 + 1] = 'e';
    tmp[PGSIZE * 3 + 2] = 'l';
    tmp[PGSIZE * 3 + 3] = 'l';
    tmp[PGSIZE * 3 + 4] = 'o';
    tmp[PGSIZE * 3 + 5] = '\0';

    syscall(SYS_copyinstr, tmp + PGSIZE * 3);

    tmp[0] = 'w';
    tmp[1] = 'o';
    tmp[2] = 'r';
    tmp[3] = 'l';
    tmp[4] = 'd';
    tmp[5] = '\0';

    syscall(SYS_copyinstr, tmp);

    while (1);
    return 0;
}
```

你需要在`trap_user_handler`中增加一些调试性输出



## 测试3: mmap_region_node 仓库管理

我们先来测试一下, 作为资源仓库, 它能不能在多核竞争的条件下保证资源申请和释放的有序性

```c
// in main.c
volatile static int started = 0;
volatile static bool over_1 = false, over_2 = false;
volatile static bool over_3 = false, over_4 = false;

void* mmap_list[N_MMAP];

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {
        
        print_init();
        printf("cpu %d is booting!\n", cpuid);
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        trap_kernel_inithart();
        
        // 初始化 + 初始状态显示
        mmap_init();
        mmap_show_nodelist();
        printf("\n");

        __sync_synchronize();
        started = 1;

        // 申请
        for(int i = 0; i < N_MMAP / 2; i++)
            mmap_list[i] = mmap_region_alloc();
        over_1 = true;

        // 屏障
        while(over_1 == false ||  over_2 == false);

        // 释放
        for(int i = 0; i < N_MMAP / 2; i++)
            mmap_region_free(mmap_list[i]);
        over_3 = true;

        // 屏障
        while (over_3 == false || over_4 == false);

        // 查看结束时的状态
        mmap_show_nodelist();        

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
        kvm_inithart();
        trap_kernel_inithart();

        // 申请
        for(int i = N_MMAP / 2; i < N_MMAP; i++)
            mmap_list[i] = mmap_region_alloc();
        over_2 = true;

        // 屏障
        while(over_1 == false || over_2 == false);

        // 释放
        for(int i = N_MMAP / 2; i < N_MMAP; i++)
            mmap_region_free(mmap_list[i]);
        over_4 = true;
    }

    while (1);
}
```


- 第一部分的输出应该是 `node X index = X` (X从0增加到255)

- 第二部分输出应该是两股输出交替 (node从0增加到255, 一股index从255减到128, 另一股index从127减到0)


## 测试4: mmap 与 munmap

我们给出了测试用例用于检测uvm_mmap()和uvm_munmap()中可能的遗漏和错误

请你理解它在测试哪些情况, 以及预期的输出是什么样的

当然, 你应该补充更多测试用例, 以确保实现的完备性

```c
// in initcode.c
#include "sys.h"

// 与内核保持一致
#define VA_MAX       (1ul << 38)
#define PGSIZE       4096
#define MMAP_END     (VA_MAX - (16 * 256 + 2) * PGSIZE)
#define MMAP_BEGIN   (MMAP_END - 64 * 256 * PGSIZE)

int main()
{
    // 建议画图理解这些地址和长度的含义

    // sys_mmap 测试 
    syscall(SYS_mmap, MMAP_BEGIN + 4 * PGSIZE, 3 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 10 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 2 * PGSIZE,  2 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 12 * PGSIZE, 1 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN + 7 * PGSIZE, 3 * PGSIZE);
    syscall(SYS_mmap, MMAP_BEGIN, 2 * PGSIZE);
    syscall(SYS_mmap, 0, 10 * PGSIZE);

    // sys_munmap 测试
    syscall(SYS_munmap, MMAP_BEGIN + 10 * PGSIZE, 5 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN, 10 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 17 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 15 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 19 * PGSIZE, 2 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 22 * PGSIZE, 1 * PGSIZE);
    syscall(SYS_munmap, MMAP_BEGIN + 21 * PGSIZE, 1 * PGSIZE);

    while(1);
    return 0;
}
```

请你在`sys_mmap()`和`sys_munmap()`中增加提示性输出

```c
    proc_t *p = myproc();
    uvm_show_mmaplist(p->mmap);
    vm_print(p->pgtbl);
    printf("\n");
```


## 测试5: 页表的复制与销毁

请你参考前4个测试点的设计, 自行决定如何测试页表的复制和销毁
