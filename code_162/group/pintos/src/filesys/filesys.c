#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();

  // Set the current working directory of the main thread so that other threads inherit it.
  struct dir *root = dir_open_root();
  thread_current()->cwd = root;
  // Set . and .. for the root directory.
  init_dir(root, root);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
  cache_flush();
}

/* Creates a file/directory named NAME with the given INITIAL_SIZE
   in directory dir.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, struct dir *dir, bool is_dir)
{
  block_sector_t inode_sector = 0;
  volatile bool success = false;
  if (is_dir) {
    success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector, true));

  } else {
    success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector, false));
  }
  /* Check for success */
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  return success;
}

/* Opens the file/directory with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Also sets is_dir.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
void *
filesys_open (struct dir* dir, const char *name, bool *is_dir)
{
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, name, &inode, is_dir);

  /* Check if the inode corresponds to a directory or a file. */
  if (*is_dir) {
    return (void *) dir_open(inode);
  }
  else {
    return (void *) file_open(inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name, struct dir *dir)
{
  bool success = dir != NULL && dir_remove (dir, name);
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
