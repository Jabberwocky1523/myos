#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"

static spinlock_t cache_lock;
static buffer_t buffers[N_BUFFER];
static buffer_node_t active_head;
static buffer_node_t inactive_head;

static void list_init(buffer_node_t *head)
{
  head->prev = head;
  head->next = head;
  head->buffer = NULL;
}

static void list_remove(buffer_node_t *node)
{
  if (node->prev == NULL || node->next == NULL)
    return;
  node->prev->next = node->next;
  node->next->prev = node->prev;
  node->prev = NULL;
  node->next = NULL;
}

void insert_node(buffer_node_t *node, bool insert_active,
                 bool insert_next)
{
  buffer_node_t *head = insert_active ? &active_head : &inactive_head;
  list_remove(node);
  if (insert_next)
  {
    node->next = head->next;
    node->prev = head;
  }
  else
  {
    node->next = head;
    node->prev = head->prev;
  }
  node->prev->next = node;
  node->next->prev = node;
}

void buffer_init(void)
{
  spinlock_init(&cache_lock, "buffer cache");
  list_init(&active_head);
  list_init(&inactive_head);
  for (uint32 i = 0; i < N_BUFFER; ++i)
  {
    buffer_t *b = &buffers[i];
    memset(b, 0, sizeof(*b));
    sleeplock_init(&b->lock, "buffer");
    b->block_num = BUFFER_BLOCK_NONE;
    b->node.buffer = b;
    insert_node(&b->node, false, false);
  }
}
buffer_t *buffer_get(uint32 block_num)
{
  if (block_num >= FS_NBLOCKS)
    panic("buffer block number");
  spinlock_acquire(&cache_lock);

  for (uint32 i = 0; i < N_BUFFER; ++i)
  {
    buffer_t *b = &buffers[i];
    if (b->block_num == block_num)
    {
      b->ref++;
      insert_node(&b->node, true, true);
      spinlock_release(&cache_lock);
      sleeplock_acquire(&b->lock);
      return b;
    }
  }

  buffer_node_t *node = inactive_head.prev;
  if (node == &inactive_head || node->buffer->ref != 0)
  {
    spinlock_release(&cache_lock);
    panic("no free buffer");
  }
  buffer_t *b = node->buffer;
  if (b->data == NULL)
    b->data = (uint8 *)pmem_alloc(true);
  b->block_num = block_num;
  b->ref = 1;
  b->valid = false;
  b->disk = false;
  insert_node(&b->node, true, true);
  spinlock_release(&cache_lock);
  sleeplock_acquire(&b->lock);
  return b;
}

void buffer_read(buffer_t *b)
{
  if (!sleeplock_holding(&b->lock))
    panic("buffer_read lock");
  if (!b->valid)
  {
    virtio_disk_rw(b, false);
    b->valid = true;
  }
}

void buffer_write(buffer_t *b)
{
  if (!sleeplock_holding(&b->lock) ||
      b->data == NULL)
    panic("buffer_write lock");
  virtio_disk_rw(b, true);
  b->valid = true;
}

void buffer_put(buffer_t *b)
{
  if (!sleeplock_holding(&b->lock))
    panic("buffer_put lock");
  spinlock_acquire(&cache_lock);
  if (b->ref == 0)
    panic("buffer_put ref");
  b->ref--;
  if (b->ref == 0)
    insert_node(&b->node, false, true);
  spinlock_release(&cache_lock);
  sleeplock_release(&b->lock);
}

uint32 buffer_freemem(uint32 buffer_count)
{
  uint32 freed = 0;
  spinlock_acquire(&cache_lock);
  buffer_node_t *node = inactive_head.prev;
  while (node != &inactive_head && freed < buffer_count)
  {
    buffer_node_t *prev = node->prev;
    buffer_t *b = node->buffer;
    if (b->ref == 0 && b->data != NULL)
    {
      pmem_free((uint64)b->data, true);
      b->data = NULL;
      b->valid = false;
      b->block_num = BUFFER_BLOCK_NONE;
      freed++;
    }
    node = prev;
  }
  spinlock_release(&cache_lock);
  return freed;
}

static void print_list(const char *name, buffer_node_t *head)
{
  printf("%s:", name);
  for (buffer_node_t *n = head->next; n != head; n = n->next)
  {
    buffer_t *b = n->buffer;
    if (b->block_num == BUFFER_BLOCK_NONE)
      printf(" [free ref=%d mem=%d]", (int)b->ref, b->data != NULL);
    else
      printf(" [block=%d ref=%d mem=%d]", (int)b->block_num,
             (int)b->ref, b->data != NULL);
  }
  printf("\n");
}

void buffer_print(void)
{
  spinlock_acquire(&cache_lock);
  print_list("active", &active_head);
  print_list("inactive", &inactive_head);
  spinlock_release(&cache_lock);
}
