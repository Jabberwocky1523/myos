#include "method.h"
#include "../lib/method.h"
#include "../arch/method.h"

#define SATP_SV39 (8UL << 60)
#define MAKE_SATP(pgtbl) (SATP_SV39 | ((uint64)(pgtbl) >> 12))

extern char KERNEL_DATA[];
extern char ALLOC_END[];

pgtbl_t kernel_pgtbl;

static bool pte_is_leaf(pte_t pte)
{
  return (pte & (PTE_R | PTE_W | PTE_X)) != 0;
}

pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
  if (pgtbl == NULL || va >= VA_MAX)
    return NULL;

  for (int level = 2; level > 0; --level) {
    pte_t *pte = &pgtbl[VA_TO_VPN(level, va)];
    if ((*pte & PTE_V) != 0) {
      if (pte_is_leaf(*pte))
        panic("leaf encountered while walking page table");
      pgtbl = (pgtbl_t)PTE_TO_PA(*pte);
    } else {
      if (!alloc)
        return NULL;
      pgtbl_t next = (pgtbl_t)pmem_alloc(true);
      *pte = PA_TO_PTE(next) | PTE_V;
      pgtbl = next;
    }
  }
  return &pgtbl[VA_TO_VPN(0, va)];
}

void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
  if ((va & PAGE_MASK) != 0 || (pa & PAGE_MASK) != 0 || len == 0 ||
      va >= VA_MAX || len > VA_MAX - va ||
      ((perm & PTE_W) != 0 && (perm & PTE_R) == 0))
    panic("vm_mappages arguments");

  uint64 mapped_len = PGROUNDUP(len);
  for (uint64 offset = 0; offset < mapped_len; offset += PAGE_SIZE) {
    pte_t *pte = vm_getpte(pgtbl, va + offset, true);
    if (pte == NULL || (*pte & PTE_V) != 0)
      panic("vm_mappages remap");
    *pte = PA_TO_PTE(pa + offset) | (uint64)perm | PTE_V;
  }
}

void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
  if ((va & PAGE_MASK) != 0 || len == 0 || va >= VA_MAX || len > VA_MAX - va)
    panic("vm_unmappages arguments");

  uint64 mapped_len = PGROUNDUP(len);
  for (uint64 offset = 0; offset < mapped_len; offset += PAGE_SIZE) {
    pte_t *pte = vm_getpte(pgtbl, va + offset, false);
    if (pte == NULL || (*pte & PTE_V) == 0 || !pte_is_leaf(*pte))
      panic("vm_unmappages missing mapping");
    if (freeit)
      pmem_free(PTE_TO_PA(*pte), false);
    *pte = 0;
  }
}

void kvm_init(void)
{
  kernel_pgtbl = (pgtbl_t)pmem_alloc(true);

  vm_mappages(kernel_pgtbl, UART0, UART0, PAGE_SIZE, PTE_R | PTE_W);
  vm_mappages(kernel_pgtbl, CLINT, CLINT, CLINT_SIZE, PTE_R | PTE_W);
  vm_mappages(kernel_pgtbl, PLIC, PLIC, PLIC_SIZE, PTE_R | PTE_W);

  uint64 text_end = PGROUNDUP((uint64)KERNEL_DATA);
  vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE,
              text_end - KERNEL_BASE, PTE_R | PTE_X);
  vm_mappages(kernel_pgtbl, text_end, text_end,
              (uint64)ALLOC_END - text_end, PTE_R | PTE_W);
}

void kvm_inithart(void)
{
  if (kernel_pgtbl == NULL)
    panic("kernel page table is not initialized");
  uint64 satp = MAKE_SATP(kernel_pgtbl);
  sfence_vma();
  w_satp(satp);
  sfence_vma();
}

static void vm_print_level(pgtbl_t pgtbl, int level)
{
  for (int i = 0; i < 512; ++i) {
    pte_t pte = pgtbl[i];
    if ((pte & PTE_V) == 0)
      continue;
    printf("level %d index %d pte %x pa %x\n", level, i,
           pte, PTE_TO_PA(pte));
    if (level > 0 && !pte_is_leaf(pte))
      vm_print_level((pgtbl_t)PTE_TO_PA(pte), level - 1);
  }
}

void vm_print(pgtbl_t pgtbl)
{
  assert(pgtbl != NULL, "vm_print null page table");
  printf("page table %x\n", (uint64)pgtbl);
  vm_print_level(pgtbl, 2);
}
