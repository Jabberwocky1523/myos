# LAB-9: 文件系统 之 文件管理与全系统整合


## 测试用例

实现`proc_exec`对系统调用的测试有很大帮助, 现在的测试流程是:

**initcode.c -> (fork + exec + wait) -> test_1 or test_2 or test_3 ...**

你只需要修改`initcode.c`中的**path**和**argv**参数即可启动不同的测试点


