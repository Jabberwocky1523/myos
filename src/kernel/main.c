#include "lib/method.h"
#include "lock/method.h"
#include "mem/method.h"

static volatile uint32 memory_ready;
static volatile uint32 tests_done;

void init(void)
{
  print_init();
  pmem_init();
  kvm_init();
}

static void test_case_2(void)
{
  uint64 pages[10];
  uint64 before = user_region.allocable;

  for (int i = 0; i < 10; ++i) {
    pages[i] = pmem_alloc(false);
    assert((pages[i] & PAGE_MASK) == 0, "allocated page is not aligned");
    assert(pages[i] >= user_region.begin && pages[i] < user_region.end,
           "allocated page outside user region");
    memset((void *)pages[i], 0xa5, PAGE_SIZE);
  }
  assert(user_region.allocable == before - 10, "page count did not decrease");

  for (int i = 0; i < 10; ++i)
    pmem_free(pages[i], false);
  assert(user_region.allocable == before, "page count did not recover");

  for (int i = 0; i < 10; ++i) {
    pages[i] = pmem_alloc(false);
    const uint64 *word = (const uint64 *)pages[i];
    for (uint64 j = 0; j < PAGE_SIZE / sizeof(uint64); ++j)
      assert(word[j] == 0, "allocated page was not cleared");
  }
  for (int i = 0; i < 10; ++i)
    pmem_free(pages[i], false);

  assert(user_region.allocable == before, "page lifecycle leaked pages");
  printf("test_case_2 passed\n");
}

static void free_empty_pagetable(pgtbl_t pgtbl, int level)
{
  for (int i = 0; i < 512; ++i) {
    pte_t pte = pgtbl[i];
    if ((pte & PTE_V) == 0)
      continue;
    assert(level > 0 && (pte & (PTE_R | PTE_W | PTE_X)) == 0,
           "temporary page table still has leaves");
    free_empty_pagetable((pgtbl_t)PTE_TO_PA(pte), level - 1);
  }
  pmem_free((uint64)pgtbl, true);
}

static void test_mapping_and_unmapping(void)
{
  pgtbl_t root = (pgtbl_t)pmem_alloc(true);
  uint64 rw_page = pmem_alloc(false);
  uint64 ro_page = pmem_alloc(false);
  const uint64 rw_va = 0x4000;
  const uint64 ro_va = 0x8000;

  vm_mappages(root, rw_va, rw_page, PAGE_SIZE, PTE_R | PTE_W);
  /* A byte length still maps its containing page. */
  vm_mappages(root, ro_va, ro_page, 1, PTE_R);

  pte_t *rw_pte = vm_getpte(root, rw_va, false);
  pte_t *ro_pte = vm_getpte(root, ro_va, false);
  assert(rw_pte != NULL && (*rw_pte & PTE_V) != 0, "RW mapping missing");
  assert(PTE_TO_PA(*rw_pte) == rw_page, "RW mapping has wrong PA");
  assert(PTE_FLAGS(*rw_pte) == (PTE_V | PTE_R | PTE_W),
         "RW mapping has wrong permissions");
  assert(ro_pte != NULL && PTE_TO_PA(*ro_pte) == ro_page,
         "read-only mapping has wrong PA");
  assert(PTE_FLAGS(*ro_pte) == (PTE_V | PTE_R),
         "read-only mapping has wrong permissions");

  vm_unmappages(root, rw_va, PAGE_SIZE, false);
  vm_unmappages(root, ro_va, 1, false);
  assert(vm_getpte(root, rw_va, false) != NULL && (*rw_pte & PTE_V) == 0,
         "RW mapping was not removed");
  assert(vm_getpte(root, ro_va, false) != NULL && (*ro_pte & PTE_V) == 0,
         "read-only mapping was not removed");

  pmem_free(rw_page, false);
  pmem_free(ro_page, false);
  free_empty_pagetable(root, 2);
  printf("test_mapping_and_unmapping passed\n");
}

static void test_kernel_mappings(void)
{
  extern char KERNEL_DATA[];
  pte_t *text = vm_getpte(kernel_pgtbl, KERNEL_BASE, false);
  pte_t *data = vm_getpte(kernel_pgtbl, (uint64)KERNEL_DATA, false);
  pte_t *uart = vm_getpte(kernel_pgtbl, UART0, false);

  assert(text != NULL && PTE_PERMS(*text) == (PTE_V | PTE_R | PTE_X),
         "kernel text permissions are too broad");
  assert(data != NULL && PTE_PERMS(*data) == (PTE_V | PTE_R | PTE_W),
         "kernel data permissions are wrong");
  assert(uart != NULL && PTE_PERMS(*uart) == (PTE_V | PTE_R | PTE_W),
         "UART mapping permissions are wrong");
  assert((*text & PTE_U) == 0 && (*data & PTE_U) == 0 && (*uart & PTE_U) == 0,
         "kernel mapping unexpectedly grants user access");
  printf("test_kernel_mappings passed\n");
}

int main(void)
{
  uint64 id = hart_id();

  if (id == 0) {
    init();
    __atomic_store_n(&memory_ready, 1, __ATOMIC_RELEASE);
  } else {
    while (__atomic_load_n(&memory_ready, __ATOMIC_ACQUIRE) == 0)
      ;
  }

  kvm_inithart();

  if (id == 0) {
    test_case_2();
    test_mapping_and_unmapping();
    test_kernel_mappings();
    __atomic_store_n(&tests_done, 1, __ATOMIC_RELEASE);
  } else {
    while (__atomic_load_n(&tests_done, __ATOMIC_ACQUIRE) == 0)
      ;
  }
  printf("lab2 hart %d ready\n", (int)id);

  for (;;)
    asm volatile("wfi");
}
