#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../proc/method.h"

static mmap_region_t mmap_pool[MMAP_REGION_COUNT];
static mmap_region_t *mmap_freelist;
static spinlock_t mmap_pool_lock;

void mmap_init(void)
{
  spinlock_init(&mmap_pool_lock, "mmap pool");
  mmap_freelist = NULL;
  for (uint32 i = MMAP_REGION_COUNT; i != 0; --i)
  {
    mmap_region_t *r = &mmap_pool[i - 1];
    memset(r, 0, sizeof(*r));
    r->index = i - 1;
    r->next = mmap_freelist;
    mmap_freelist = r;
  }
}

mmap_region_t *mmap_region_alloc(void)
{
  spinlock_acquire(&mmap_pool_lock);
  mmap_region_t *r = mmap_freelist;
  if (r != NULL)
    mmap_freelist = r->next;
  spinlock_release(&mmap_pool_lock);
  if (r == NULL)
    panic("mmap region pool exhausted");
  uint32 index = r->index;
  memset(r, 0, sizeof(*r));
  r->index = index;
  return r;
}

void mmap_region_free(mmap_region_t *r)
{
  if (r == NULL || r < mmap_pool || r >= mmap_pool + MMAP_REGION_COUNT)
    panic("invalid mmap region free");
  spinlock_acquire(&mmap_pool_lock);
  for (mmap_region_t *p = mmap_freelist; p != NULL; p = p->next)
    if (p == r)
      panic("double mmap region free");
  r->begin = r->end = 0;
  r->perm = 0;
  r->next = mmap_freelist;
  mmap_freelist = r;
  spinlock_release(&mmap_pool_lock);
}

void mmap_show_nodelist(void)
{
  spinlock_acquire(&mmap_pool_lock);
  for (mmap_region_t *r = mmap_freelist; r != NULL; r = r->next)
    printf("node %d index = %d\n", (int)r->index, (int)r->index);
  spinlock_release(&mmap_pool_lock);
}

void uvm_show_mmaplist(mmap_region_t *mmap)
{
  mmap_region_t *tmp = mmap;
  printf("\nalloced mmap_space:\n");
  if (tmp == NULL)
    printf("empty\n");
  while (tmp != NULL)
  {
    printf("alloced mmap_region: %x ~ %x\n", tmp->begin, tmp->end);
    tmp = tmp->next;
  }
}
void mmap_merge(mmap_region_t *a, mmap_region_t *b, bool keep_a)
{
  assert(a != NULL && b != NULL && a->perm == b->perm &&
             (a->end == b->begin || b->end == a->begin),
         "invalid mmap merge");
  mmap_region_t *keep = keep_a ? a : b;
  mmap_region_t *drop = keep_a ? b : a;
  if (drop->begin < keep->begin)
    keep->begin = drop->begin;
  if (drop->end > keep->end)
    keep->end = drop->end;
  mmap_region_free(drop);
}

uint64 uvm_mmap_find(mmap_region_t *head, uint64 len,
                     mmap_region_t **last_out, mmap_region_t **next_out)
{
  uint64 candidate = MMAP_BEGIN;
  mmap_region_t *last = NULL;
  mmap_region_t *next = head;
  while (next != NULL)
  {
    if (candidate <= next->begin && len <= next->begin - candidate)
      break;
    candidate = next->end;
    last = next;
    next = next->next;
  }
  if (candidate > MMAP_END || len > MMAP_END - candidate)
    return 0;
  if (last_out != NULL)
    *last_out = last;
  if (next_out != NULL)
    *next_out = next;
  return candidate;
}

static int mmap_request(uint64 *begin, uint64 len, mmap_region_t **last,
                        mmap_region_t **next)
{
  proc_t *p = myproc();
  *last = NULL;
  *next = p->mmap;
  if (*begin == 0)
  {
    *begin = uvm_mmap_find(p->mmap, len, last, next);
    return *begin == 0 ? -1 : 0;
  }
  if ((*begin & PAGE_MASK) != 0 || *begin < MMAP_BEGIN ||
      *begin > MMAP_END || len > MMAP_END - *begin)
    return -1;
  while (*next != NULL && (*next)->end <= *begin)
  {
    *last = *next;
    *next = (*next)->next;
  }
  if ((*last != NULL && (*last)->end > *begin) ||
      (*next != NULL && *begin + len > (*next)->begin))
    return -1;
  return 0;
}

uint64 uvm_mmap(uint64 begin, uint32 npages, int perm)
{
  proc_t *p = myproc();
  if (p == NULL || npages == 0 ||
      (uint64)npages > (MMAP_END - MMAP_BEGIN) / PAGE_SIZE ||
      (perm & PTE_R) == 0 || (perm & ~(PTE_R | PTE_W | PTE_X)) != 0 ||
      ((perm & PTE_W) != 0 && (perm & PTE_R) == 0))
    return (uint64)-1;
  uint64 len = (uint64)npages * PAGE_SIZE;
  mmap_region_t *last;
  mmap_region_t *next;
  if (mmap_request(&begin, len, &last, &next) < 0)
    return (uint64)-1;

  mmap_region_t *region = mmap_region_alloc();
  region->begin = begin;
  region->end = begin + len;
  region->perm = perm;
  uint64 va;
  for (va = begin; va < region->end; va += PAGE_SIZE)
  {
    uint64 pa = pmem_try_alloc(false);
    if (pa == 0)
      break;
    vm_mappages(p->pgtbl, va, pa, PAGE_SIZE, perm | PTE_U);
  }
  if (va != region->end)
  {
    if (va > begin)
      vm_unmappages(p->pgtbl, begin, va - begin, true);
    mmap_region_free(region);
    return (uint64)-1;
  }

  region->next = next;
  if (last == NULL)
    p->mmap = region;
  else
    last->next = region;

  if (next != NULL && region->end == next->begin &&
      region->perm == next->perm)
  {
    region->end = next->end;
    region->next = next->next;
    mmap_region_free(next);
  }
  if (last != NULL && last->end == region->begin &&
      last->perm == region->perm)
  {
    last->end = region->end;
    last->next = region->next;
    mmap_region_free(region);
  }
  return begin;
}

static int range_is_mapped(mmap_region_t *r, uint64 begin, uint64 end)
{
  uint64 cursor = begin;
  while (r != NULL && r->end <= cursor)
    r = r->next;
  while (cursor < end)
  {
    if (r == NULL || r->begin > cursor)
      return 0;
    if (r->end > cursor)
      cursor = r->end < end ? r->end : end;
    r = r->next;
  }
  return 1;
}

int uvm_munmap(uint64 begin, uint32 npages)
{
  proc_t *p = myproc();
  if (p == NULL || npages == 0 || (begin & PAGE_MASK) != 0 ||
      begin < MMAP_BEGIN || (uint64)npages > (MMAP_END - MMAP_BEGIN) / PAGE_SIZE)
    return -1;
  uint64 len = (uint64)npages * PAGE_SIZE;
  if (begin > MMAP_END || len > MMAP_END - begin)
    return -1;
  uint64 end = begin + len;
  if (!range_is_mapped(p->mmap, begin, end))
    return -1;

  mmap_region_t *split = NULL;
  for (mmap_region_t *r = p->mmap; r != NULL; r = r->next)
    if (r->begin < begin && r->end > end)
      split = mmap_region_alloc();

  vm_unmappages(p->pgtbl, begin, len, true);
  mmap_region_t **link = &p->mmap;
  while (*link != NULL && (*link)->begin < end)
  {
    mmap_region_t *r = *link;
    if (r->end <= begin)
    {
      link = &r->next;
      continue;
    }
    if (r->begin < begin && r->end > end)
    {
      split->begin = end;
      split->end = r->end;
      split->perm = r->perm;
      split->next = r->next;
      r->end = begin;
      r->next = split;
      break;
    }
    if (r->begin < begin)
    {
      r->end = begin;
      link = &r->next;
      continue;
    }
    if (r->end > end)
    {
      r->begin = end;
      break;
    }
    *link = r->next;
    mmap_region_free(r);
  }
  return 0;
}
