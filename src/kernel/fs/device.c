#include "mod.h"
#include "../lib/method.h"
#include "../mem/method.h"
#include "../proc/method.h"

static device_t device_table[N_DEVICE];

/* Read one line from the system console. */
uint32 device_stdin_read(uint32 len, uint64 dst, bool is_user_dst)
{
  return cons_read(len, dst, is_user_dst);
}

/* Write a byte stream to standard output. */
uint32 device_stdout_write(uint32 len, uint64 src, bool is_user_src)
{
  return cons_write(len, src, is_user_src);
}

/* Write a byte stream to the diagnostic console. */
uint32 device_stderr_write(uint32 len, uint64 src, bool is_user_src)
{
  return cons_write(len, src, is_user_src);
}

/* Fill a destination with an unlimited stream of zero bytes. */
uint32 device_zero_read(uint32 len, uint64 dst, bool is_user_dst)
{
  uint8 zero[64];
  memset(zero, 0, sizeof(zero));
  proc_t *p = myproc();
  uint32 done = 0;
  while (done < len)
  {
    uint32 count = len - done > sizeof(zero) ? sizeof(zero) : len - done;
    if (is_user_dst)
    {
      if (p == NULL || uvm_copyout(p->pgtbl, dst + done,
                                   (uint64)zero, count) < 0)
        return (uint32)-1;
    }
    else
      memmove((void *)(dst + done), zero, count);
    done += count;
  }
  return done;
}

/* Report immediate end-of-file for the null device. */
uint32 device_null_read(uint32 len, uint64 dst, bool is_user_dst)
{
  (void)len;
  (void)dst;
  (void)is_user_dst;
  return 0;
}

/* Discard all bytes written to the null device. */
uint32 device_null_write(uint32 len, uint64 src, bool is_user_src)
{
  (void)src;
  (void)is_user_src;
  return len;
}

/* Consume one prompt and print the deliberately simple gpt0 response. */
uint32 device_gpt0_write(uint32 len, uint64 src, bool is_user_src)
{
  char question[128];
  uint32 count = len > sizeof(question) ? sizeof(question) : len;
  if (is_user_src)
  {
    proc_t *p = myproc();
    if (p == NULL || uvm_copyin(p->pgtbl, (uint64)question, src, count) < 0)
      return (uint32)-1;
  }
  else if (count != 0)
    memmove(question, (void *)src, count);
  (void)question;
  char answer[] = "gpt0: I am a tiny device, so my answer is 42.\n";
  cons_write(sizeof(answer) - 1, (uint64)answer, false);
  return len;
}

/* Install one major number in the device dispatch table. */
void device_register(uint32 index, char *name, device_io_fn_t read,
                     device_io_fn_t write)
{
  if (index >= N_DEVICE || name == NULL)
    panic("device_register");
  device_table[index].name = name;
  device_table[index].read = read;
  device_table[index].write = write;
}

/* Register built-in devices and ensure their /dev nodes exist. */
void device_init(void)
{
  memset(device_table, 0, sizeof(device_table));
  device_register(DEVICE_STDIN, "stdin", device_stdin_read, NULL);
  device_register(DEVICE_STDOUT, "stdout", NULL, device_stdout_write);
  device_register(DEVICE_STDERR, "stderr", NULL, device_stderr_write);
  device_register(DEVICE_ZERO, "zero", device_zero_read, NULL);
  device_register(DEVICE_NULL, "null", device_null_read, device_null_write);
  device_register(DEVICE_GPT0, "gpt0", NULL, device_gpt0_write);

  inode_t *ip = path_to_inode("/dev");
  if (ip == NULL)
    ip = path_create_inode("/dev", INODE_TYPE_DIRECTORY,
                           INODE_MAJOR_DEFAULT, INODE_MINOR_DEFAULT);
  if (ip == NULL)
    panic("create /dev");
  inode_put(ip);

  char *paths[] = {"/dev/stdin", "/dev/stdout", "/dev/stderr",
                   "/dev/zero", "/dev/null", "/dev/gpt0"};
  uint16 majors[] = {DEVICE_STDIN, DEVICE_STDOUT, DEVICE_STDERR,
                     DEVICE_ZERO, DEVICE_NULL, DEVICE_GPT0};
  for (uint32 i = 0; i < sizeof(majors) / sizeof(majors[0]); ++i)
  {
    ip = path_to_inode(paths[i]);
    if (ip == NULL)
      ip = path_create_inode(paths[i], INODE_TYPE_DEVICE, majors[i], 0);
    if (ip == NULL)
      panic("create device node");
    inode_put(ip);
  }
}

/* Check that a device major implements every requested access direction. */
bool device_open_check(uint16 major, uint32 open_mode)
{
  if (major >= N_DEVICE || device_table[major].name == NULL)
    return false;
  if ((open_mode & OPEN_READ) != 0 && device_table[major].read == NULL)
    return false;
  if ((open_mode & OPEN_WRITE) != 0 && device_table[major].write == NULL)
    return false;
  return true;
}

/* Dispatch a read to a validated device major. */
uint32 device_read_data(uint16 major, uint32 len, uint64 dst,
                        bool is_user_dst)
{
  if (major >= N_DEVICE || device_table[major].read == NULL)
    return (uint32)-1;
  return device_table[major].read(len, dst, is_user_dst);
}

/* Dispatch a write to a validated device major. */
uint32 device_write_data(uint16 major, uint32 len, uint64 src,
                         bool is_user_src)
{
  if (major >= N_DEVICE || device_table[major].write == NULL)
    return (uint32)-1;
  return device_table[major].write(len, src, is_user_src);
}
