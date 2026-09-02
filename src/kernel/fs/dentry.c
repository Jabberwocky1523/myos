#include "method.h"
#include "../lib/method.h"
#include "../lock/method.h"
#include "../mem/method.h"
#include "../proc/method.h"

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

  uint32 offset = free_offset == INVALID_INODE_NUM ? ip->disk_info.size : free_offset;
  if (offset + sizeof(entry) > BLOCK_SIZE)
    return -1;

  inode_t *child = inode_num == ip->inode_num ? ip : inode_get(inode_num);
  if (child == NULL)
    return -1;
  if (!sleeplock_holding(&child->slk))
    panic("dentry_create child unlocked");
  if (child->disk_info.nlink == 0xffffU)
  {
    if (child != ip)
      inode_put(child);
    return -1;
  }
  child->disk_info.nlink++;
  inode_rw(child->inode_num, &child->disk_info, true);

  memset(&entry, 0, sizeof(entry));
  entry.inode_num = inode_num;
  memmove(entry.name, name, name_len);
  if (inode_write_data(ip, offset, sizeof(entry), &entry, false) !=
      (int)sizeof(entry))
  {
    child->disk_info.nlink--;
    inode_rw(child->inode_num, &child->disk_info, true);
    if (child != ip)
      inode_put(child);
    return -1;
  }
  if (child != ip)
    inode_put(child);
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
    inode_t *child = inode_num == ip->inode_num ? ip : inode_get(inode_num);
    if (child == NULL)
      return INVALID_INODE_NUM;
    if (!sleeplock_holding(&child->slk))
      panic("dentry_delete child unlocked");
    if (child->disk_info.nlink == 0)
    {
      if (child != ip)
        inode_put(child);
      return INVALID_INODE_NUM;
    }
    memset(&entry, 0, sizeof(entry));
    entry.inode_num = INVALID_INODE_NUM;
    if (inode_write_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
    {
      if (child != ip)
        inode_put(child);
      return INVALID_INODE_NUM;
    }
    child->disk_info.nlink--;
    inode_rw(child->inode_num, &child->disk_info, true);
    if (child != ip)
      inode_put(child);
    return inode_num;
  }
  return INVALID_INODE_NUM;
}

/* Find the offset of one exact name and inode pair in a locked directory. */
uint32 dentry_search_2(inode_t *ip, uint32 inode_num, char *name)
{
  if (ip == NULL || name == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_search_2 state");
  dentry_t entry;
  for (uint32 offset = 0; offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return INVALID_INODE_NUM;
    if (entry.inode_num == inode_num &&
        strncmp(entry.name, name, DENTRY_NAME_SIZE) == 0)
      return offset;
  }
  return INVALID_INODE_NUM;
}

/* Compact all live directory entries into a kernel or user buffer. */
int dentry_transmit(inode_t *ip, uint64 dst, uint32 len,
                    bool is_user_dst)
{
  if (ip == NULL || !sleeplock_holding(&ip->slk) ||
      ip->disk_info.type != INODE_TYPE_DIRECTORY)
    panic("dentry_transmit state");
  uint32 done = 0;
  dentry_t entry;
  for (uint32 offset = 0; offset + sizeof(entry) <= ip->disk_info.size;
       offset += sizeof(entry))
  {
    if (inode_read_data(ip, offset, sizeof(entry), &entry, false) !=
        (int)sizeof(entry))
      return -1;
    if (entry.inode_num == INVALID_INODE_NUM)
      continue;
    if (sizeof(entry) > len - done)
      break;
    if (is_user_dst)
    {
      proc_t *p = myproc();
      uint8 user_entry[sizeof(entry)];
      memset(user_entry, 0, sizeof(user_entry));
      memmove(user_entry, entry.name, DENTRY_NAME_SIZE);
      memmove(user_entry + DENTRY_NAME_SIZE, &entry.inode_num,
              sizeof(entry.inode_num));
      if (p == NULL || uvm_copyout(p->pgtbl, dst + done,
                                   (uint64)user_entry,
                                   sizeof(user_entry)) < 0)
        return -1;
    }
    else
      memmove((void *)(dst + done), &entry, sizeof(entry));
    done += sizeof(entry);
  }
  return (int)done;
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

/* Resolve an absolute or cwd-relative path to its target or parent inode. */
inode_t *__path_to_inode(char *path, char *name, bool find_parent_inode)
{
  if (path == NULL)
    return NULL;
  inode_t *ip;
  if (path[0] == '/')
    ip = inode_get(ROOT_INODE);
  else
  {
    proc_t *p = myproc();
    ip = p == NULL || p->cwd == NULL ? NULL : inode_dup(p->cwd);
  }
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

/* Resolve a path to its target inode. */
inode_t *path_to_inode(char *path)
{
  return __path_to_inode(path, NULL, false);
}

/* Resolve a path to its parent and return the final name. */
inode_t *path_to_parent_inode(char *path, char *name)
{
  if (name == NULL)
    return NULL;
  return __path_to_inode(path, name, true);
}

/* Build an absolute path for an inode by walking parent directory entries. */
int inode_to_path(inode_t *ip, char *path, uint32 len)
{
  if (ip == NULL || path == NULL || len < 2)
    return -1;
  uint32 cursor = len - 1;
  path[cursor] = '\0';
  inode_t *current = inode_dup(ip);
  while (current != NULL && current->inode_num != ROOT_INODE)
  {
    uint32 child_num = current->inode_num;
    inode_lock(current);
    uint32 parent_num = dentry_search(current, "..");
    inode_unlock(current);
    inode_put(current);
    if (parent_num == INVALID_INODE_NUM)
      return -1;
    inode_t *parent = inode_get(parent_num);
    if (parent == NULL)
      return -1;
    inode_lock(parent);
    char component[DENTRY_NAME_SIZE];
    component[0] = '\0';
    dentry_t entry;
    for (uint32 offset = 0;
         offset + sizeof(entry) <= parent->disk_info.size;
         offset += sizeof(entry))
    {
      if (inode_read_data(parent, offset, sizeof(entry), &entry, false) !=
          (int)sizeof(entry))
        break;
      if (entry.inode_num == child_num &&
          strncmp(entry.name, ".", DENTRY_NAME_SIZE) != 0 &&
          strncmp(entry.name, "..", DENTRY_NAME_SIZE) != 0)
      {
        memmove(component, entry.name, DENTRY_NAME_SIZE);
        component[DENTRY_NAME_SIZE - 1] = '\0';
        break;
      }
    }
    inode_unlock(parent);
    if (component[0] == '\0')
    {
      inode_put(parent);
      return -1;
    }
    uint32 component_len = strlen(component);
    if (component_len + 1 > cursor)
    {
      inode_put(parent);
      return -1;
    }
    cursor -= component_len;
    memmove(path + cursor, component, component_len);
    path[--cursor] = '/';
    current = parent;
  }
  if (current != NULL)
    inode_put(current);
  if (cursor == len - 1)
    path[--cursor] = '/';
  return (int)cursor;
}

/* Create and publish a new inode at a path, returning it unlocked. */
inode_t *path_create_inode(char *path, uint16 type, uint16 major,
                           uint16 minor)
{
  char name[DENTRY_NAME_SIZE];
  inode_t *parent = path_to_parent_inode(path, name);
  if (parent == NULL || name[0] == '\0' ||
      strncmp(name, ".", DENTRY_NAME_SIZE) == 0 ||
      strncmp(name, "..", DENTRY_NAME_SIZE) == 0)
  {
    inode_put(parent);
    return NULL;
  }
  inode_t *child = inode_create(type, major, minor);
  if (child == NULL)
  {
    inode_put(parent);
    return NULL;
  }
  inode_lock(child);
  inode_lock(parent);
  bool ok = parent->disk_info.type == INODE_TYPE_DIRECTORY &&
            dentry_search(parent, name) == INVALID_INODE_NUM;
  if (ok && type == INODE_TYPE_DIRECTORY)
    ok = dentry_create(child, child->inode_num, ".") >= 0 &&
         dentry_create(child, parent->inode_num, "..") >= 0;
  if (ok)
    ok = dentry_create(parent, child->inode_num, name) >= 0;
  if (!ok && type == INODE_TYPE_DIRECTORY)
  {
    dentry_t invalid;
    memset(&invalid, 0, sizeof(invalid));
    invalid.inode_num = INVALID_INODE_NUM;
    if (dentry_search(child, "..") == parent->inode_num)
    {
      inode_write_data(child, sizeof(dentry_t), sizeof(invalid),
                       &invalid, false);
      parent->disk_info.nlink--;
      inode_rw(parent->inode_num, &parent->disk_info, true);
    }
    if (dentry_search(child, ".") == child->inode_num)
    {
      inode_write_data(child, 0, sizeof(invalid), &invalid, false);
      child->disk_info.nlink--;
      inode_rw(child->inode_num, &child->disk_info, true);
    }
  }
  inode_unlock(parent);
  inode_unlock(child);
  inode_put(parent);
  if (!ok)
  {
    inode_put(child);
    return NULL;
  }
  return child;
}

/* Add a non-directory hard link at a new path. */
int path_link(char *old_path, char *new_path)
{
  inode_t *old = path_to_inode(old_path);
  char name[DENTRY_NAME_SIZE];
  inode_t *parent = path_to_parent_inode(new_path, name);
  if (old == NULL || parent == NULL)
  {
    inode_put(old);
    inode_put(parent);
    return -1;
  }
  inode_lock(old);
  inode_lock(parent);
  int result = -1;
  if (old->disk_info.type != INODE_TYPE_DIRECTORY &&
      parent->disk_info.type == INODE_TYPE_DIRECTORY &&
      dentry_search(parent, name) == INVALID_INODE_NUM &&
      dentry_create(parent, old->inode_num, name) >= 0)
    result = 0;
  inode_unlock(parent);
  inode_unlock(old);
  inode_put(parent);
  inode_put(old);
  return result;
}

/* Remove a file or an empty directory and balance all directory links. */
int path_unlink(char *path)
{
  char name[DENTRY_NAME_SIZE];
  inode_t *parent = path_to_parent_inode(path, name);
  inode_t *child = path_to_inode(path);
  if (parent == NULL || child == NULL || child->inode_num == ROOT_INODE)
  {
    inode_put(parent);
    inode_put(child);
    return -1;
  }
  inode_lock(child);
  inode_lock(parent);
  bool empty = true;
  if (child->disk_info.type == INODE_TYPE_DIRECTORY)
  {
    dentry_t entry;
    for (uint32 offset = 0;
         offset + sizeof(entry) <= child->disk_info.size;
         offset += sizeof(entry))
    {
      if (inode_read_data(child, offset, sizeof(entry), &entry, false) !=
          (int)sizeof(entry))
      {
        empty = false;
        break;
      }
      if (entry.inode_num != INVALID_INODE_NUM &&
          strncmp(entry.name, ".", DENTRY_NAME_SIZE) != 0 &&
          strncmp(entry.name, "..", DENTRY_NAME_SIZE) != 0)
      {
        empty = false;
        break;
      }
    }
  }
  int result = -1;
  if (empty && dentry_search(parent, name) == child->inode_num)
  {
    if (child->disk_info.type == INODE_TYPE_DIRECTORY)
    {
      dentry_t invalid;
      memset(&invalid, 0, sizeof(invalid));
      invalid.inode_num = INVALID_INODE_NUM;
      inode_write_data(child, sizeof(dentry_t), sizeof(invalid),
                       &invalid, false);
      parent->disk_info.nlink--;
      inode_rw(parent->inode_num, &parent->disk_info, true);
      inode_write_data(child, 0, sizeof(invalid), &invalid, false);
      child->disk_info.nlink--;
      inode_rw(child->inode_num, &child->disk_info, true);
    }
    if (dentry_delete(parent, name) != INVALID_INODE_NUM)
      result = 0;
  }
  inode_unlock(parent);
  inode_unlock(child);
  inode_put(parent);
  inode_put(child);
  return result;
}
