#include "help.h"
#include "syscall_arch.h"
#include "syscall_num.h"

/* Request a new user heap top. */
uint64 sys_brk(uint64 new_heap_top) { return __syscall1(SYS_brk, new_heap_top); }
/* Create a user mapping. */
uint64 sys_mmap(uint64 start, uint32 len) { return __syscall2(SYS_mmap, start, len); }
/* Remove a user mapping. */
uint64 sys_munmap(uint64 start, uint32 len) { return __syscall2(SYS_munmap, start, len); }
/* Clone the current process. */
uint32 sys_fork(void) { return __syscall0(SYS_fork); }
/* Reap one child process. */
uint32 sys_wait(uint32 *exit_state) { return __syscall1(SYS_wait, (long)exit_state); }
/* Terminate the current process. */
void sys_exit(uint32 exit_state) { __syscall1(SYS_exit, exit_state); for (;;) asm volatile("wfi"); }
/* Sleep for a number of timer ticks. */
uint32 sys_sleep(uint32 ntick) { return __syscall1(SYS_sleep, ntick); }
/* Return the current process id. */
uint32 sys_getpid(void) { return __syscall0(SYS_getpid); }
/* Replace the current process image. */
uint32 sys_exec(char *path, char **argv) { return __syscall2(SYS_exec, (long)path, (long)argv); }
/* Open a path with the requested mode. */
uint32 sys_open(char *path, uint32 open_mode) { return __syscall2(SYS_open, (long)path, open_mode); }
/* Close a descriptor. */
uint32 sys_close(uint32 fd) { return __syscall1(SYS_close, fd); }
/* Read bytes into a user buffer. */
uint32 sys_read(uint32 fd, uint32 len, void *addr) { return __syscall3(SYS_read, fd, len, (long)addr); }
/* Write bytes from a user buffer. */
uint32 sys_write(uint32 fd, uint32 len, void *addr) { return __syscall3(SYS_write, fd, len, (long)addr); }
/* Move a shared file offset. */
uint32 sys_lseek(uint32 fd, uint32 offset, uint32 flag) { return __syscall3(SYS_lseek, fd, offset, flag); }
/* Duplicate a descriptor. */
uint32 sys_dup(uint32 fd) { return __syscall1(SYS_dup, fd); }
/* Copy file metadata to user memory. */
uint32 sys_fstat(uint32 fd, file_stat_t *stat) { return __syscall2(SYS_fstat, fd, (long)stat); }
/* Copy live directory entries to user memory. */
uint32 sys_get_dentries(uint32 fd, dentry_t *buf, uint32 buf_len) { return __syscall3(SYS_get_dentries, fd, (long)buf, buf_len); }
/* Create a directory. */
uint32 sys_mkdir(char *path) { return __syscall1(SYS_mkdir, (long)path); }
/* Change the current working directory. */
uint32 sys_chdir(char *new_path) { return __syscall1(SYS_chdir, (long)new_path); }
/* Print the absolute current working directory. */
uint32 sys_print_cwd(void) { return __syscall0(SYS_print_cwd); }
/* Create a hard link. */
uint32 sys_link(char *old_path, char *new_path) { return __syscall2(SYS_link, (long)old_path, (long)new_path); }
/* Remove a directory entry. */
uint32 sys_unlink(char *path) { return __syscall1(SYS_unlink, (long)path); }
