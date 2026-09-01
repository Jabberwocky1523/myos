#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"

/* Find a named entry in a locked directory inode. */
uint32 dentry_search(inode_t *ip, char *name)
{
  if (ip == NULL || name == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_search state");
  dentry_t entry;
  for (uint32 offset = 0;
       offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return INVALID_INODE_NUM;
    if (entry.inode_num != INVALID_INODE_NUM &&
        strncmp(entry.name, name, DENTRY_NAME_SIZE) == 0)
      return entry.inode_num;
  }
  return INVALID_INODE_NUM;
}

/* Insert a directory entry and publish the target inode link. */
int dentry_create(inode_t *ip, uint32 inode_num, char *name)
{
  if (ip == NULL || name == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_create state");
  uint32 name_len = strlen(name);
  if (name_len == 0 || name_len >= DENTRY_NAME_SIZE ||
      inode_num >= superblock.ninodes)
    return -1;

  dentry_t entry;
  uint32 free_offset = INVALID_INODE_NUM;
  for (uint32 offset = 0;
       offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return -1;
    if (entry.inode_num == INVALID_INODE_NUM)
    {
      if (free_offset == INVALID_INODE_NUM)
        free_offset = offset;
    }
    else if (strncmp(entry.name, name, DENTRY_NAME_SIZE) == 0)
      return -1;
  }

  uint32 offset = free_offset == INVALID_INODE_NUM ?
                  ip->disk_info.size : free_offset;
  if (offset + sizeof(entry) > BLOCK_SIZE)
    return -1;

  inode_t *child = NULL;
  if (inode_num == ip->inode_num)
  {
    if (ip->disk_info.nlink == 0xffffU)
      return -1;
    ip->disk_info.nlink++;
    inode_rw(ip->inode_num, &ip->disk_info, true);
  }
  else
  {
    child = inode_get(inode_num);
    if (child == NULL)
      return -1;
    inode_lock(child);
    if (child->disk_info.nlink == 0xffffU)
    {
      inode_unlock(child);
      inode_put(child);
      return -1;
    }
    child->disk_info.nlink++;
    inode_rw(child->inode_num, &child->disk_info, true);
    inode_unlock(child);
    inode_put(child);
  }

  memset(&entry, 0, sizeof(entry));
  entry.inode_num = inode_num;
  memmove(entry.name, name, name_len);
  if (inode_write_data(ip, offset, sizeof(entry), &entry, false) !=
      (int)sizeof(entry))
  {
    if (inode_num == ip->inode_num)
    {
      ip->disk_info.nlink--;
      inode_rw(ip->inode_num, &ip->disk_info, true);
    }
    else
    {
      child = inode_get(inode_num);
      if (child != NULL)
      {
        inode_lock(child);
        child->disk_info.nlink--;
        inode_rw(child->inode_num, &child->disk_info, true);
        inode_unlock(child);
        inode_put(child);
      }
    }
    return -1;
  }
  return (int)offset;
}

/* Remove a directory entry and drop the target inode link. */
uint32 dentry_delete(inode_t *ip, char *name)
{
  if (ip == NULL || name == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_delete state");
  if (strncmp(name, ".", DENTRY_NAME_SIZE) == 0 ||
      strncmp(name, "..", DENTRY_NAME_SIZE) == 0)
    return INVALID_INODE_NUM;

  dentry_t entry;
  for (uint32 offset = 0;
       offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return INVALID_INODE_NUM;
    if (entry.inode_num == INVALID_INODE_NUM ||
        strncmp(entry.name, name, DENTRY_NAME_SIZE) != 0)
      continue;
    uint32 inode_num = entry.inode_num;
    memset(&entry, 0, sizeof(entry));
    entry.inode_num = INVALID_INODE_NUM;
    if (inode_write_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return INVALID_INODE_NUM;

    inode_t *child = inode_get(inode_num);
    if (child == NULL)
      return INVALID_INODE_NUM;
    inode_lock(child);
    if (child->disk_info.nlink == 0)
    {
      inode_unlock(child);
      inode_put(child);
      return INVALID_INODE_NUM;
    }
    child->disk_info.nlink--;
    inode_rw(child->inode_num, &child->disk_info, true);
    inode_unlock(child);
    inode_put(child);
    return inode_num;
  }
  return INVALID_INODE_NUM;
}

/* Print all live entries in a locked directory inode. */
void dentry_print(inode_t *ip)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_print state");
  dentry_t entry;
  printf("directory inode %d:\n", (int)ip->inode_num);
  for (uint32 offset = 0;
       offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      panic("dentry_print read");
    if (entry.inode_num != INVALID_INODE_NUM)
      printf("  %s -> %d\n", entry.name, (int)entry.inode_num);
  }
}

/* Copy the next normalized path component and return the remainder. */
char *get_element(char *path, char *name)
{
  if (path == NULL || name == NULL)
    return NULL;
  while (*path == '/')
    path++;
  uint32 length = 0;
  while (*path != '\0' && *path != '/')
  {
    if (length + 1 >= DENTRY_NAME_SIZE)
    {
      name[0] = '\0';
      return NULL;
    }
    name[length++] = *path++;
  }
  name[length] = '\0';
  while (*path == '/')
    path++;
  return path;
}

/* Resolve an absolute path to either its target or its parent inode. */
inode_t *__path_to_inode(char *path, char *name, bool find_parent_inode)
{
  if (path == NULL || path[0] != '/')
    return NULL;
  inode_t *ip = inode_get(ROOT_INODE);
  if (ip == NULL)
    return NULL;

  char element[DENTRY_NAME_SIZE];
  char *cursor = path;
  for (;;)
  {
    cursor = get_element(cursor, element);
    if (cursor == NULL)
    {
      inode_put(ip);
      return NULL;
    }
    if (element[0] == '\0')
    {
      if (find_parent_inode)
      {
        inode_put(ip);
        return NULL;
      }
      return ip;
    }
    bool last = *cursor == '\0';
    if (find_parent_inode && last)
    {
      uint32 length = strlen(element);
      memmove(name, element, length + 1);
      return ip;
    }

    inode_lock(ip);
    if (ip->disk_info.type != INODE_TYPE_DIRECTORY)
    {
      inode_unlock(ip);
      inode_put(ip);
      return NULL;
    }
    uint32 inode_num = dentry_search(ip, element);
    inode_unlock(ip);
    inode_put(ip);
    if (inode_num == INVALID_INODE_NUM)
      return NULL;
    ip = inode_get(inode_num);
    if (ip == NULL)
      return NULL;
    if (last)
      return ip;
  }
}

/* Resolve an absolute path to its target inode. */
inode_t *path_to_inode(char *path)
{
  return __path_to_inode(path, NULL, false);
}

/* Resolve an absolute path to its parent and return the final name. */
inode_t *path_to_parent_inode(char *path, char *name)
{
  if (name == NULL)
    return NULL;
  return __path_to_inode(path, name, true);
}
