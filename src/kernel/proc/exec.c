#include "method.h"
#include "../fs/mod.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"

#define ELF_PROGRAM_LOAD 1U
#define ELF_FLAG_EXEC 1U
#define ELF_FLAG_WRITE 2U
#define ELF_FLAG_READ 4U

typedef struct elf_program_header
{
  uint32 type;
  uint32 flags;
  uint64 offset;
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz;
  uint64 align;
} elf_program_header_t;

/* Load one file segment into pages already mapped in a new user page table. */
void load_segment(inode_t *ip, pgtbl_t pgtbl, uint64 seg_start,
                  uint64 va_start, uint32 len)
{
  if (ip == NULL || pgtbl == NULL || !sleeplock_holding(&ip->slk))
    panic("load_segment state");
  uint32 done = 0;
  while (done < len)
  {
    uint64 va = va_start + done;
    pte_t *pte = vm_getpte(pgtbl, va, false);
    if (pte == NULL || (*pte & (PTE_V | PTE_U)) != (PTE_V | PTE_U))
      panic("load_segment mapping");
    uint32 count = PAGE_SIZE - (uint32)(va & PAGE_MASK);
    if (count > len - done)
      count = len - done;
    uint64 dst = PTE_TO_PA(*pte) + (va & PAGE_MASK);
    if (inode_read_data(ip, (uint32)(seg_start + done), count,
                        (void *)dst, false) != (int)count)
      panic("load_segment read");
    done += count;
  }
}

/* Validate ELF program headers, map their pages, and copy file contents. */
uint64 prepare_heap(pgtbl_t new_pgtbl, inode_t *ip, elf_header_t *eh)
{
  if (new_pgtbl == NULL || ip == NULL || eh == NULL ||
      !sleeplock_holding(&ip->slk) ||
      eh->phentsize != sizeof(elf_program_header_t))
    return (uint64)-1;
  uint64 heap_top = USER_BASE;
  for (uint32 i = 0; i < eh->phnum; ++i)
  {
    elf_program_header_t ph;
    uint64 header_offset = eh->phoff + i * sizeof(ph);
    if (header_offset > 0xffffffffUL ||
        inode_read_data(ip, (uint32)header_offset, sizeof(ph),
                        &ph, false) != (int)sizeof(ph))
      return (uint64)-1;
    if (ph.type != ELF_PROGRAM_LOAD)
      continue;
    if (ph.memsz < ph.filesz || ph.vaddr < heap_top ||
        (ph.vaddr & PAGE_MASK) != 0 || ph.memsz > MMAP_BEGIN - ph.vaddr ||
        ph.filesz > 0xffffffffUL || ph.offset > 0xffffffffUL)
      return (uint64)-1;

    int perm = 0;
    if ((ph.flags & ELF_FLAG_READ) != 0)
      perm |= PTE_R;
    if ((ph.flags & ELF_FLAG_WRITE) != 0)
      perm |= PTE_W;
    if ((ph.flags & ELF_FLAG_EXEC) != 0)
      perm |= PTE_X;
    if ((perm & (PTE_R | PTE_X)) == 0)
      return (uint64)-1;
    if (ph.vaddr > heap_top &&
        uvm_heap_grow(new_pgtbl, heap_top,
                      (uint32)(ph.vaddr - heap_top), perm) == (uint64)-1)
      return (uint64)-1;
    heap_top = ph.vaddr;
    if (ph.memsz > 0xffffffffUL ||
        uvm_heap_grow(new_pgtbl, heap_top, (uint32)ph.memsz,
                      perm) == (uint64)-1)
      return (uint64)-1;
    if (ph.filesz != 0)
      load_segment(ip, new_pgtbl, ph.offset, ph.vaddr,
                   (uint32)ph.filesz);
    heap_top = ph.vaddr + ph.memsz;
  }
  return heap_top == USER_BASE ? (uint64)-1 : heap_top;
}

/* Map a one-page user stack and copy argv strings plus its pointer vector. */
uint64 prepare_stack(pgtbl_t new_pgtbl, char **argv, int *arg_count)
{
  if (new_pgtbl == NULL || argv == NULL || arg_count == NULL)
    return (uint64)-1;
  uint64 stack_page = pmem_try_alloc(false);
  if (stack_page == 0)
    return (uint64)-1;
  vm_mappages(new_pgtbl, USER_STACK, stack_page, PAGE_SIZE,
              PTE_R | PTE_W | PTE_U);

  uint64 user_argv[ELF_MAXARGS + 1];
  uint64 sp = USER_STACK_TOP;
  uint32 argc = 0;
  for (; argc < ELF_MAXARGS && argv[argc] != NULL; ++argc)
  {
    uint32 length = strlen(argv[argc]) + 1;
    if (length > ELF_MAXARG_LEN || length > sp - USER_STACK)
      goto fail;
    sp -= length;
    sp &= ~15UL;
    if (sp < USER_STACK ||
        uvm_copyout(new_pgtbl, sp, (uint64)argv[argc], length) < 0)
      goto fail;
    user_argv[argc] = sp;
  }
  if (argc == ELF_MAXARGS && argv[argc] != NULL)
    goto fail;
  user_argv[argc] = 0;
  uint64 vector_size = (argc + 1) * sizeof(uint64);
  sp = (sp - vector_size) & ~15UL;
  if (sp < USER_STACK ||
      uvm_copyout(new_pgtbl, sp, (uint64)user_argv,
                  (uint32)vector_size) < 0)
    goto fail;
  *arg_count = (int)argc;
  return sp;

fail:
  vm_unmappages(new_pgtbl, USER_STACK, PAGE_SIZE, true);
  return (uint64)-1;
}

/* Atomically replace the current process with a validated ELF image. */
int proc_exec(char *path, char **argv)
{
  proc_t *p = myproc();
  inode_t *ip = path_to_inode(path);
  if (p == NULL || ip == NULL)
  {
    inode_put(ip);
    return -1;
  }
  inode_lock(ip);
  elf_header_t eh;
  if (ip->disk_info.type != INODE_TYPE_FILE ||
      inode_read_data(ip, 0, sizeof(eh), &eh, false) != (int)sizeof(eh) ||
      eh.magic != ELF_MAGIC)
  {
    inode_unlock(ip);
    inode_put(ip);
    return -1;
  }

  pgtbl_t new_pgtbl = proc_pgtbl_init((uint64)p->trapframe);
  uint64 new_heap_top = prepare_heap(new_pgtbl, ip, &eh);
  inode_unlock(ip);
  inode_put(ip);
  if (new_heap_top == (uint64)-1)
  {
    uvm_destroy_pgtbl(new_pgtbl);
    return -1;
  }
  pte_t *entry_pte = vm_getpte(new_pgtbl, eh.entry, false);
  if (eh.entry < USER_BASE || eh.entry >= new_heap_top ||
      entry_pte == NULL || (*entry_pte & (PTE_V | PTE_U | PTE_X)) !=
                           (PTE_V | PTE_U | PTE_X))
  {
    uvm_destroy_pgtbl(new_pgtbl);
    return -1;
  }
  int argc;
  uint64 sp = prepare_stack(new_pgtbl, argv, &argc);
  if (sp == (uint64)-1)
  {
    uvm_destroy_pgtbl(new_pgtbl);
    return -1;
  }

  pgtbl_t old_pgtbl = p->pgtbl;
  mmap_region_t *old_mmap = p->mmap;
  p->pgtbl = new_pgtbl;
  p->heap_top = new_heap_top;
  p->ustack_npage = 1;
  p->mmap = NULL;
  p->trapframe->epc = eh.entry;
  p->trapframe->sp = sp;
  p->trapframe->a0 = (uint64)argc;
  p->trapframe->a1 = sp;
  p->name = "exec";

  uvm_destroy_pgtbl(old_pgtbl);
  while (old_mmap != NULL)
  {
    mmap_region_t *next = old_mmap->next;
    mmap_region_free(old_mmap);
    old_mmap = next;
  }
  return argc;
}
