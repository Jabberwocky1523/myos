#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../proc/method.h"

#define VIRTIO_MMIO_MAGIC_VALUE       0x000
#define VIRTIO_MMIO_VERSION           0x004
#define VIRTIO_MMIO_DEVICE_ID         0x008
#define VIRTIO_MMIO_VENDOR_ID         0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES   0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES   0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_GUEST_PAGE_SIZE   0x028
#define VIRTIO_MMIO_QUEUE_SEL         0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX     0x034
#define VIRTIO_MMIO_QUEUE_NUM         0x038
#define VIRTIO_MMIO_QUEUE_ALIGN       0x03c
#define VIRTIO_MMIO_QUEUE_PFN         0x040
#define VIRTIO_MMIO_QUEUE_READY       0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY      0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS  0x060
#define VIRTIO_MMIO_INTERRUPT_ACK     0x064
#define VIRTIO_MMIO_STATUS            0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW    0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH   0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW   0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH  0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW   0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH  0x0a4

#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_DRIVER_OK   4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

#define VIRTIO_BLK_F_RO          5
#define VIRTIO_BLK_F_SCSI        7
#define VIRTIO_BLK_F_CONFIG_WCE 11
#define VIRTIO_BLK_F_MQ         12
#define VIRTIO_F_ANY_LAYOUT     27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX 29

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

#define REG(offset) (*(volatile uint32 *)(VIRTIO0 + (offset)))

static struct {
  spinlock_t lock;
  virtq_desc_t *desc;
  virtq_avail_t *avail;
  volatile virtq_used_t *used;
  bool free[VIRTIO_NUM];
  uint16 used_idx;
  struct {
    buffer_t *b;
    uint8 status;
    virtio_blk_req_t req;
  } info[VIRTIO_NUM];
} disk;

static uint64 dma_address(const void *ptr)
{
  uint64 va = (uint64)ptr;
  pte_t *pte = vm_getpte(NULL, va, false);
  if (pte == NULL || (*pte & PTE_V) == 0 ||
      (*pte & (PTE_R | PTE_W | PTE_X)) == 0)
    panic("virtio dma address");
  return PTE_TO_PA(*pte) + (va & PAGE_MASK);
}

static void write_address(uint32 low_reg, const void *address)
{
  uint64 pa = dma_address(address);
  REG(low_reg) = (uint32)pa;
  REG(low_reg + 4) = (uint32)(pa >> 32);
}

void virtio_disk_init(void)
{
  uint32 magic = REG(VIRTIO_MMIO_MAGIC_VALUE);
  uint32 version = REG(VIRTIO_MMIO_VERSION);
  uint32 device = REG(VIRTIO_MMIO_DEVICE_ID);
  uint32 vendor = REG(VIRTIO_MMIO_VENDOR_ID);
  if (magic != 0x74726976 || (version != 1 && version != 2) ||
      device != 2 || vendor == 0)
  {
    printf("virtio probe magic=%x version=%d device=%d vendor=%x\n",
           (uint64)magic, (int)version, (int)device, (uint64)vendor);
    panic("virtio disk not found");
  }

  REG(VIRTIO_MMIO_STATUS) = 0;
  uint32 status = VIRTIO_CONFIG_S_ACKNOWLEDGE;
  REG(VIRTIO_MMIO_STATUS) = status;
  status |= VIRTIO_CONFIG_S_DRIVER;
  REG(VIRTIO_MMIO_STATUS) = status;

  REG(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 0;
  REG(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
  uint32 features = REG(VIRTIO_MMIO_DEVICE_FEATURES);
  features &= ~(1U << VIRTIO_BLK_F_RO);
  features &= ~(1U << VIRTIO_BLK_F_SCSI);
  features &= ~(1U << VIRTIO_BLK_F_CONFIG_WCE);
  features &= ~(1U << VIRTIO_BLK_F_MQ);
  features &= ~(1U << VIRTIO_F_ANY_LAYOUT);
  features &= ~(1U << VIRTIO_RING_F_EVENT_IDX);
  features &= ~(1U << VIRTIO_RING_F_INDIRECT_DESC);
  REG(VIRTIO_MMIO_DRIVER_FEATURES) = features;
  if (version == 2)
  {
    REG(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 1;
    uint32 high_features = REG(VIRTIO_MMIO_DEVICE_FEATURES);
    if ((high_features & 1U) == 0)
      panic("virtio version feature");
    REG(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 1;
    REG(VIRTIO_MMIO_DRIVER_FEATURES) = high_features;
    REG(VIRTIO_MMIO_DEVICE_FEATURES_SEL) = 0;
    REG(VIRTIO_MMIO_DRIVER_FEATURES_SEL) = 0;
  }

  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  REG(VIRTIO_MMIO_STATUS) = status;
  if ((REG(VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
    panic("virtio feature negotiation");

  REG(VIRTIO_MMIO_QUEUE_SEL) = 0;
  if (REG(VIRTIO_MMIO_QUEUE_NUM_MAX) < VIRTIO_NUM)
    panic("virtio queue unavailable");
  REG(VIRTIO_MMIO_QUEUE_NUM) = VIRTIO_NUM;
  if (version == 1)
  {
    uint64 page_a = pmem_alloc(true);
    uint64 page_b = pmem_alloc(true);
    uint64 low_page = page_a < page_b ? page_a : page_b;
    uint64 high_page = page_a < page_b ? page_b : page_a;
    if (low_page + PAGE_SIZE != high_page)
      panic("virtio legacy queue pages");
    disk.desc = (virtq_desc_t *)low_page;
    disk.avail = (virtq_avail_t *)(low_page +
        sizeof(virtq_desc_t) * VIRTIO_NUM);
    disk.used = (volatile virtq_used_t *)(low_page + PAGE_SIZE);
    REG(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PAGE_SIZE;
    REG(VIRTIO_MMIO_QUEUE_ALIGN) = PAGE_SIZE;
    REG(VIRTIO_MMIO_QUEUE_PFN) = (uint32)(dma_address(disk.desc) >> 12);
  }
  else
  {
    if (REG(VIRTIO_MMIO_QUEUE_READY) != 0)
      panic("virtio queue already ready");
    disk.desc = (virtq_desc_t *)pmem_alloc(true);
    disk.avail = (virtq_avail_t *)pmem_alloc(true);
    disk.used = (volatile virtq_used_t *)pmem_alloc(true);
    write_address(VIRTIO_MMIO_QUEUE_DESC_LOW, disk.desc);
    write_address(VIRTIO_MMIO_DRIVER_DESC_LOW, disk.avail);
    write_address(VIRTIO_MMIO_DEVICE_DESC_LOW, (const void *)disk.used);
    REG(VIRTIO_MMIO_QUEUE_READY) = 1;
  }

  for (uint32 i = 0; i < VIRTIO_NUM; ++i)
    disk.free[i] = true;
  spinlock_init(&disk.lock, "virtio disk");
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  REG(VIRTIO_MMIO_STATUS) = status;
}

int alloc_desc(void)
{
  for (int i = 0; i < (int)VIRTIO_NUM; ++i)
    if (disk.free[i])
    {
      disk.free[i] = false;
      return i;
    }
  return -1;
}

void free_desc(int i)
{
  if (i < 0 || i >= (int)VIRTIO_NUM || disk.free[i])
    panic("virtio free desc");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = true;
}

void free_chain(int i)
{
  for (;;)
  {
    uint16 flags = disk.desc[i].flags;
    int next = disk.desc[i].next;
    free_desc(i);
    if ((flags & VRING_DESC_F_NEXT) == 0)
      break;
    i = next;
  }
}

int alloc3_desc(int idx[3])
{
  for (int i = 0; i < 3; ++i)
  {
    idx[i] = alloc_desc();
    if (idx[i] < 0)
    {
      for (int j = 0; j < i; ++j)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void virtio_disk_rw(buffer_t *b, bool write)
{
  if (b == NULL || b->data == NULL || !sleeplock_holding(&b->lock))
    panic("virtio rw buffer");

  spinlock_acquire(&disk.lock);
  int idx[3];
  while (alloc3_desc(idx) < 0)
    proc_sleep(&disk.free[0], &disk.lock);

  virtio_blk_req_t *req = &disk.info[idx[0]].req;
  req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
  req->reserved = 0;
  req->sector = (uint64)b->block_num * (BLOCK_SIZE / 512U);

  disk.desc[idx[0]].addr = dma_address(req);
  disk.desc[idx[0]].len = sizeof(*req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = (uint16)idx[1];

  disk.desc[idx[1]].addr = dma_address(b->data);
  disk.desc[idx[1]].len = BLOCK_SIZE;
  disk.desc[idx[1]].flags = VRING_DESC_F_NEXT |
      (write ? 0 : VRING_DESC_F_WRITE);
  disk.desc[idx[1]].next = (uint16)idx[2];

  disk.info[idx[0]].status = 0xff;
  disk.desc[idx[2]].addr = dma_address(&disk.info[idx[0]].status);
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
  disk.desc[idx[2]].next = 0;

  disk.info[idx[0]].b = b;
  b->disk = true;
  disk.avail->ring[disk.avail->idx % VIRTIO_NUM] = (uint16)idx[0];
  __sync_synchronize();
  disk.avail->idx++;
  __sync_synchronize();
  REG(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

  while (b->disk)
    proc_sleep(b, &disk.lock);
  if (disk.info[idx[0]].status != 0)
    panic("virtio disk status");
  disk.info[idx[0]].b = NULL;
  free_chain(idx[0]);
  proc_wakeup(&disk.free[0]);
  spinlock_release(&disk.lock);
}

void virtio_disk_intr(void)
{
  spinlock_acquire(&disk.lock);
  REG(VIRTIO_MMIO_INTERRUPT_ACK) =
      REG(VIRTIO_MMIO_INTERRUPT_STATUS) & 3U;
  __sync_synchronize();

  while (disk.used_idx != disk.used->idx)
  {
    __sync_synchronize();
    uint32 id = disk.used->ring[disk.used_idx % VIRTIO_NUM].id;
    if (id >= VIRTIO_NUM || disk.info[id].b == NULL)
      panic("virtio used descriptor");
    if (disk.info[id].status != 0)
      panic("virtio request failed");
    buffer_t *b = disk.info[id].b;
    b->disk = false;
    proc_wakeup(b);
    disk.used_idx++;
  }
  spinlock_release(&disk.lock);
}
