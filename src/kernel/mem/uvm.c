#include "method.h"
#include "../lib/method.h"

static int user_page_pa(pgtbl_t pgtbl, uint64 va, int required)
{
  if (pgtbl == NULL || va >= VA_MAX)
    return -1;
  pte_t *pte = vm_getpte(pgtbl, va, false);
  if (pte == NULL || (*pte & (PTE_V | PTE_U)) != (PTE_V | PTE_U) ||
      (*pte & required) != (uint64)required)
    return -1;
  return 0;
}

static uint64 translated_pa(pgtbl_t pgtbl, uint64 va)
{
  pte_t *pte = vm_getpte(pgtbl, va, false);
  return PTE_TO_PA(*pte) + (va & PAGE_MASK);
}

int uvm_copyin(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
  if (len != 0 && (src >= VA_MAX || (uint64)len > VA_MAX - src))
    return -1;
  while (len != 0)
  {
    if (user_page_pa(pgtbl, src, PTE_R) < 0)
      return -1;
    uint32 n = (uint32)(PAGE_SIZE - (src & PAGE_MASK));
    if (n > len)
      n = len;
    memmove((void *)dst, (void *)translated_pa(pgtbl, src), n);
    dst += n;
    src += n;
    len -= n;
  }
  return 0;
}

int uvm_copyout(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
  if (len != 0 && (dst >= VA_MAX || (uint64)len > VA_MAX - dst))
    return -1;
  while (len != 0)
  {
    if (user_page_pa(pgtbl, dst, PTE_W) < 0)
      return -1;
    uint32 n = (uint32)(PAGE_SIZE - (dst & PAGE_MASK));
    if (n > len)
      n = len;
    memmove((void *)translated_pa(pgtbl, dst), (void *)src, n);
    dst += n;
    src += n;
    len -= n;
  }
  return 0;
}

int uvm_copyin_str(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 maxlen)
{
  char *out = (char *)dst;
  for (uint32 copied = 0; copied < maxlen; ++copied, ++src)
  {
    if (src >= VA_MAX || user_page_pa(pgtbl, src, PTE_R) < 0)
      return -1;
    char c = *(char *)translated_pa(pgtbl, src);
    out[copied] = c;
    if (c == '\0')
      return 0;
  }
  return -1;
}

uint64 uvm_heap_grow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len)
{
  if (cur_heap_top < USER_HEAP_BASE || cur_heap_top > MMAP_BEGIN ||
      (uint64)len > MMAP_BEGIN - cur_heap_top)
    return (uint64)-1;
  uint64 new_top = cur_heap_top + len;
  uint64 begin = PGROUNDUP(cur_heap_top);
  uint64 end = PGROUNDUP(new_top);
  uint64 va;
  for (va = begin; va < end; va += PAGE_SIZE)
  {
    uint64 pa = pmem_try_alloc(false);
    if (pa == 0)
      break;
    vm_mappages(pgtbl, va, pa, PAGE_SIZE, PTE_R | PTE_W | PTE_U);
  }
  if (va != end)
  {
    if (va > begin)
      vm_unmappages(pgtbl, begin, va - begin, true);
    return (uint64)-1;
  }
  return new_top;
}

uint64 uvm_heap_ungrow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len)
{
  if (cur_heap_top < USER_HEAP_BASE || (uint64)len >
      cur_heap_top - USER_HEAP_BASE)
    return (uint64)-1;
  uint64 new_top = cur_heap_top - len;
  uint64 begin = PGROUNDUP(new_top);
  uint64 end = PGROUNDUP(cur_heap_top);
  if (begin < end)
    vm_unmappages(pgtbl, begin, end - begin, true);
  return new_top;
}

int64 uvm_ustack_grow(pgtbl_t pgtbl, uint64 old_ustack_npage,
                      uint64 fault_addr)
{
  if (old_ustack_npage == 0 || old_ustack_npage >
      (USER_STACK_TOP - USER_STACK_BOTTOM) / PAGE_SIZE ||
      fault_addr >= USER_STACK_TOP)
    return -1;
  uint64 old_bottom = USER_STACK_TOP - old_ustack_npage * PAGE_SIZE;
  uint64 new_bottom = PGROUNDDOWN(fault_addr);
  if (new_bottom >= old_bottom || new_bottom < USER_STACK_BOTTOM)
    return -1;
  uint64 va;
  for (va = new_bottom; va < old_bottom; va += PAGE_SIZE)
  {
    uint64 pa = pmem_try_alloc(false);
    if (pa == 0)
      break;
    vm_mappages(pgtbl, va, pa, PAGE_SIZE, PTE_R | PTE_W | PTE_U);
  }
  if (va != old_bottom)
  {
    if (va > new_bottom)
      vm_unmappages(pgtbl, new_bottom, va - new_bottom, true);
    return -1;
  }
  return (int64)((USER_STACK_TOP - new_bottom) / PAGE_SIZE);
}

void destroy_pgtbl(pgtbl_t pgtbl, uint32 level)
{
  if (pgtbl == NULL || level == 0)
    return;
  for (uint32 i = 0; i < 512; ++i)
  {
    pte_t pte = pgtbl[i];
    if ((pte & PTE_V) == 0)
      continue;
    if ((pte & (PTE_R | PTE_W | PTE_X)) != 0)
    {
      if ((pte & PTE_U) != 0)
        pmem_free(PTE_TO_PA(pte), false);
    }
    else
    {
      destroy_pgtbl((pgtbl_t)PTE_TO_PA(pte), level - 1);
      pmem_free(PTE_TO_PA(pte), true);
    }
    pgtbl[i] = 0;
  }
}

void uvm_destroy_pgtbl(pgtbl_t pgtbl)
{
  if (pgtbl == NULL)
    return;
  destroy_pgtbl(pgtbl, 3);
  pmem_free((uint64)pgtbl, true);
}

int copy_range(pgtbl_t old, pgtbl_t new, uint64 begin, uint64 end)
{
  if ((begin & PAGE_MASK) != 0 || end < begin || end > VA_MAX)
    return -1;
  end = PGROUNDUP(end);
  uint64 va;
  for (va = begin; va < end; va += PAGE_SIZE)
  {
    pte_t *oldpte = vm_getpte(old, va, false);
    if (oldpte == NULL || (*oldpte & (PTE_V | PTE_U)) != (PTE_V | PTE_U))
      break;
    uint64 pa = pmem_try_alloc(false);
    if (pa == 0)
      break;
    memmove((void *)pa, (void *)PTE_TO_PA(*oldpte), PAGE_SIZE);
    vm_mappages(new, va, pa, PAGE_SIZE,
                (int)(PTE_FLAGS(*oldpte) & ~PTE_V));
  }
  if (va == end)
    return 0;
  for (uint64 undo = begin; undo < va; undo += PAGE_SIZE)
    vm_unmappages(new, undo, PAGE_SIZE, true);
  return -1;
}

int uvm_copy_pgtbl(pgtbl_t old, pgtbl_t new, uint64 heap_top,
                   uint64 ustack_npage, mmap_region_t *mmap)
{
  if (heap_top < USER_HEAP_BASE || heap_top > MMAP_BEGIN ||
      ustack_npage == 0)
    return -1;
  if (copy_range(old, new, USER_BASE, heap_top) < 0)
    return -1;
  uint64 stack_begin = USER_STACK_TOP - ustack_npage * PAGE_SIZE;
  if (copy_range(old, new, stack_begin, USER_STACK_TOP) < 0)
    goto fail;
  for (mmap_region_t *r = mmap; r != NULL; r = r->next)
    if (copy_range(old, new, r->begin, r->end) < 0)
      goto fail;
  return 0;

fail:
  destroy_pgtbl(new, 3);
  return -1;
}
