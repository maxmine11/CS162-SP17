#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

static char dot_dot[] = "..";
static char dot[] = ".";

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
    bool is_dir;                        /* Is the dir entry a directory? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode, bool *is_dir)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  *is_dir = e.is_dir;
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool is_dir)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  e.is_dir = is_dir;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Check if it's a directory and if it is, make sure it's empty */
  if (e.is_dir) {
    char name[NAME_MAX + 1];
    /* We don't want to advance offset in the dir passed in. */
    struct dir *dir_copy = dir_reopen(dir);
    /* dir_readdir returns false if no entries. */
    if (!dir_readdir(dir_copy, name)) {
      goto done;
    }
    dir_close(dir_copy);
  } 

  /* Can't delete . and .. */
  if (name == dot_dot || name == dot)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use && e.name != dot_dot && e.name != dot)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
next call will return the next file name part. Returns 1 if successful, 0 at
end of string, -1 for a too-long file name part. */
int get_next_part (char part[NAME_MAX + 1], const char **srcp) {
  const char *src = *srcp;
  char *dst = part;
  /* Skip leading slashes. If it’s all slashes, we’re done. */
  while (*src == '/') {
    src++;
  }
  if (*src == '\0') {
    return 0;
  }
  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX) {
      *dst++ = *src;
    }
    else {
      return -1;
    }
    src++;
  }
  *dst = '\0';
  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

/* Initializes the . and .. entries of child directory with parent. Ex. parent/child */
void init_dir(struct dir *parent, struct dir *child) {
  /* Set .. */
  struct inode *i = dir_get_inode(parent);
  block_sector_t parent_inode_number = inode_get_inumber(i);
  dir_add(child, dot_dot, parent_inode_number, true);
  /* Set . */
  i = dir_get_inode(child);
  block_sector_t child_inode_number = inode_get_inumber(i);
  dir_add(child, dot, child_inode_number, true);
}

/* Given a path, sets name to the string corressponding to the last item in the path
 * and dir to the directory that item would be in. If the the last item == the directory
 * that item is in, then it must be the root directory.
 * Ex. path = emilio/sam/mulie/jp => dir = mulie && name = jp
 * Returns if the path was valid.
 */

bool path_resolution(struct dir **dir, char name[NAME_MAX + 1], char *path) {
  // Structs we'll need
  struct inode *inode;
  bool success = false;
  bool is_dir;
  int num;
  if (*path == '/')
    // Absolute Path
    *dir = dir_open_root();
  else
    // Relative Path
    *dir = dir_reopen(thread_current()->cwd);

  // We want to keep checking until we reach the end of the string or error
  num = get_next_part(name, (const char **) &path);
  while (num == 1) {
    success = dir_lookup (*dir, name, &inode, &is_dir);
    // We want to switch dir if what we find is a directory.
    if (success && is_dir) {
      dir_close (*dir);
      *dir = dir_open(inode); 
    }
    // We don't find what we are looking for
    if (!success) {
      // If we are at the end, it's ok
      if (*path == '\0')
        return true;
      // If we are not at the end, not ok
      else
        return false;
    }
    // Last case: If we find a file
    if (success && !is_dir) {
      // If we are at the end, it's ok
      if (*path == '\0')
        return true;
      // If we are not at the end, not ok
      else
        return false;
    }
    num = get_next_part(name, (const char **) &path);
  }
  return num == 0 ? true : false;
}
