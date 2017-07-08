#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define MAX_INODE_SIZE 0x800000

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    block_sector_t direct[12];
    block_sector_t indirect;
    block_sector_t doubly_indirect;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[112];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}
/* In-memory inode. */
struct inode
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
  static block_sector_t
  byte_to_sector (const struct inode *inode, off_t pos)
  {
    ASSERT (inode != NULL);
    block_sector_t sector = 0;
    block_sector_t buffer[128] = {0};
    char buff[BLOCK_SECTOR_SIZE];
    cache_read(inode->sector, buff);
    struct inode_disk *data = (struct inode_disk *) buff;

    
    ASSERT (sizeof(buffer) == BLOCK_SECTOR_SIZE);
    if (pos <= data->length)
    {
      if (pos >= 0 && pos < 12 * BLOCK_SECTOR_SIZE)
      {
        sector = data->direct[ pos / BLOCK_SECTOR_SIZE ]; 
      }
    
      if (pos >= 12 * BLOCK_SECTOR_SIZE && pos < (12 + 128) * BLOCK_SECTOR_SIZE)
      {
        memset (buffer, 0, BLOCK_SECTOR_SIZE); // clear the buffer before use
        //block_read (fs_device, inode->data.indirect, buffer);
        cache_read(data->indirect, buffer);
        pos -= (12 * BLOCK_SECTOR_SIZE);
        sector = buffer[ pos / BLOCK_SECTOR_SIZE ];
      }

      if (pos >= (12 + 128) * BLOCK_SECTOR_SIZE && pos < MAX_INODE_SIZE)
      {
        memset (buffer, 0, BLOCK_SECTOR_SIZE); // clear the buffer befor use
        pos -= (12 + 128) * BLOCK_SECTOR_SIZE;
        //block_read (fs_device, inode->data.doubly_indirect, buffer);
        cache_read(data->doubly_indirect, buffer);
        sector = buffer[ pos / (128 * BLOCK_SECTOR_SIZE) ];
        memset (buffer, 0, BLOCK_SECTOR_SIZE); // clear the buffer befor use
        //block_read (fs_device, sector, buffer);
        cache_read(sector, buffer);
        pos /= BLOCK_SECTOR_SIZE;
        sector = buffer[ (pos % (128 * BLOCK_SECTOR_SIZE)) % 128 ];
      }
      return sector;
    }
    else
    {
      //printf ("byte to sector error!\n");
      return 1;
    }
  }

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
/* allocate blocks */
static void zero_out (size_t size, block_sector_t sectors[]);
static size_t allocate_direct_blocks (size_t sectors, struct inode_disk *disk_inode);
static size_t allocate_indirect_blocks (size_t sectors, struct inode_disk *disk_inode);
static size_t allocate_doubly_indirect_blocks (size_t sectors, struct inode_disk *disk_inode);
static bool allocate_blocks (size_t sectors, struct inode_disk *disk_inode);
/* free blocks */
static void free_direct (struct inode *inode, size_t sectors);
static void free_indirect (struct inode *inode, size_t sectors);
static void free_doubly_indirect (struct inode *inode, size_t sectors);
static void free_all_sectors (struct inode *inode, size_t sectors);
/* print sectors */
// static void print_sectors(block_sector_t sectors[], size_t size);

/* resize inode and copy back data if need be */
static bool resize_inode (struct inode *inode, off_t resize, bool copy_back);

static bool resize_inode (struct inode *inode, off_t resize, bool copy_back)
{
  bool success = false;
  volatile size_t sectors = bytes_to_sectors (resize);
  //printf ("resize by: %d\n", resize);
  //printf ("sectors to resize by: %d\n", sectors);
  volatile off_t written = 0;
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;

  
  if (!copy_back)
  {
    //printf ("NO copying:\n");
    success = allocate_blocks (sectors, data);
    data->length = resize;
    if (!success)
      printf ("FAILED TO ALLOCATE!\n");
  }
  else
  {
    //printf ("copying:\n");
    volatile off_t old_size = data->length;
    void * old_buffer = malloc (old_size);
    // printf ("memsetting: %d", old_size);
    //memset (old_buffer, 0, old_size);
    off_t bytes_copied = inode_read_at (inode, old_buffer, old_size, 0); // save prev data
    if (bytes_copied == old_size)
    {
      free_all_sectors (inode, bytes_to_sectors (old_size));
      success = allocate_blocks (sectors, data); // allocate new sectors
      
      if (success)
      {
        //printf ("success: old length: %d\n", old_size);
        data->length = resize;
        written = inode_write_at (inode, old_buffer, old_size, 0); // recurse to write back old data 
        success = success && written == old_size;
        //printf ("copied back bytes: %d: expected: %d\n", written, old_size);
      }
      else
      {
        //printf ("FAILED TO ALLOCATE!\n");
      }
    }
    free (old_buffer);
  }
  return success;
}
  

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (allocate_blocks (sectors, disk_inode))
      {
        cache_write (sector, disk_inode);
        success = true;
      }
      free (disk_inode);
    }
  return success;
}

/* debugging: prinf sector */
// static void print_sectors (block_sector_t sectors[], size_t cnt)
// {
//   size_t i;
//   for (i = 0; i < cnt; i++)
//   {
//     //printf ("\n sector: %d", sectors[i]);
//   }
// }

/* allocate direct blocks */
size_t
allocate_direct_blocks (size_t sectors, struct inode_disk *disk_inode)
{
  size_t allocated = 0;
  if (sectors < 12)
  {
    if (free_map_allocate_array (12, sectors, disk_inode->direct))
    {
      zero_out (sectors, disk_inode->direct);
      allocated = sectors;
    }
  }
  else
  {
   if (free_map_allocate_array (12, 12, disk_inode->direct))
   {
     zero_out (12, disk_inode->direct);
     allocated = 12;
   }
  }
  return allocated;
}

/* allocate indirect blocks */
size_t
allocate_indirect_blocks (size_t sectors, struct inode_disk *disk_inode)
{
  size_t allocated = 0;
  block_sector_t buffer[128] = {0};
  if (free_map_allocate (1, &disk_inode->indirect))
  {
    if (sectors < 128)
    {
      if (free_map_allocate_array (128, sectors, buffer))
      {
        zero_out (sectors, buffer);
        allocated = sectors;
      }
    }
    else
    {
      if (free_map_allocate_array (128, 128, buffer))
      {
        zero_out (128, buffer);
        allocated = 128;
      }
    }
    cache_write (disk_inode->indirect, buffer);
  }
  return allocated;
}

size_t
allocate_doubly_indirect_blocks (size_t sectors, struct inode_disk *disk_inode)
{
  block_sector_t buffer[128] = {0};
  ASSERT (sectors < 128 * 127);
  
  size_t allocated = 0;
  size_t left = sectors;
  
  if (free_map_allocate (1, &disk_inode->doubly_indirect))
  {
    size_t i = 0;
    while (left > 0)
    {
      block_sector_t doubly_buffer[128] = {0};
      if (left < 128)
      {
        if (free_map_allocate_array (128, left, doubly_buffer))
        {
          zero_out (sectors, doubly_buffer);
          allocated += left;
          left -= left;
        } else {
          return 0;
        }
      }
      else
      {
        if (free_map_allocate_array (128, 128, doubly_buffer))
        {
          zero_out (128, doubly_buffer);
          allocated += 128;
          left -= 128;
        } else {
          return 0;
        }
      }
      /* update buffer */
      if (free_map_allocate (1, &buffer[i]))
      {
        cache_write (buffer[i], doubly_buffer);
        i++;
      } else {
        return 0;
      }
    }
    cache_write (disk_inode->doubly_indirect, buffer);
  }
  return allocated;
}

/* allocates and zero outs SECTORS blocks */
bool
allocate_blocks (size_t sectors, struct inode_disk *disk_inode)
{ 
  /* too big to allocate */
  if (sectors > 128 * 128)
    return false;
  
  volatile size_t allocated_so_far = 0;
  volatile size_t allocated_remaining = sectors > 0 ? sectors : 1;
  
  volatile size_t allocated = 0;
  bool success = false;
  if (allocated_so_far == 0 && allocated_remaining > 0)
  { 
    if ((allocated = allocate_direct_blocks (allocated_remaining, disk_inode)) > 0)
    {
      allocated_remaining -= allocated;
      allocated_so_far += allocated;
      success = true;
    }
    else
    {
      success = false;
    }
  }
  
  if (success && allocated_so_far == 12 && allocated_remaining > 0)
  {
    if ((allocated = allocate_indirect_blocks (allocated_remaining, disk_inode)) > 0)
    {
      allocated_remaining -= allocated;
      allocated_so_far += allocated;
      success = true;
    }
    else
    {
      success = false;
    }
  }
  
  if (success && allocated_so_far == 128 + 12 && allocated_remaining > 0)
  {
    if ((allocated = allocate_doubly_indirect_blocks (allocated_remaining, disk_inode)) > 0)
    {
      allocated_remaining -= allocated;
      allocated_so_far += allocated;
      success = true;
    }
    else
    {
      success = false;
    }
  }
  return success;
}

/* zero out SIZE sectors */
void
zero_out (size_t size, block_sector_t sectors[])
{
  static char zeros[BLOCK_SECTOR_SIZE] = {0};
  size_t i;
  for (i = 0; i < size; i++)
    cache_write (sectors[i], zeros);
}  

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;


  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed)
        {
          // Fetch inode disk (data) from cache.
          char buff[BLOCK_SECTOR_SIZE];
          cache_read(inode->sector, buff);
          struct inode_disk *data = (struct inode_disk *) buff;
          free_map_release (inode->sector, 1);
          size_t sectors = bytes_to_sectors (data->length);
          free_all_sectors (inode, sectors);
        }

      free (inode);
    }
}

/* free direct blocks */
static void 
free_direct (struct inode *inode, size_t sectors)
{
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;
  free_map_release_array (data->direct, sectors);
}

/* free indirect blocks */
static void 
free_indirect (struct inode *inode, size_t sectors)
{
  block_sector_t buffer[128] = {0};
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;
  cache_read (data->indirect, buffer);
  free_map_release_array (buffer, sectors);
}

/* free doubly indirect blocks */
static void 
free_doubly_indirect (struct inode *inode, size_t sectors)
{
  block_sector_t buffer[128] = {0};
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;
  cache_read (data->doubly_indirect, buffer);
  
  size_t free_left = sectors;
  size_t i = 0;
  
  while (free_left > 0)
  {
    block_sector_t doubly_buffer[128] = {0};
    cache_read (buffer[i], doubly_buffer);
    i++;
    if (free_left < 128)
    {
      free_map_release_array (doubly_buffer, free_left);
      free_left -= free_left;
    }
    else
    {
      free_map_release_array (doubly_buffer, 128);
      free_left -= 128;
    }
  }
  
}

/* frees all sectors in inode */
static void
free_all_sectors (struct inode *inode, size_t sectors)
{
  ASSERT (sectors <= 128 * 128)
  if (sectors <= 12)
  {
    free_direct (inode, sectors);
  }
  if (sectors > 12 && sectors <= 12 + 128)
  {
    free_direct (inode, 12);
    free_indirect (inode, sectors - 12);
  }
  if (sectors > 12 + 128)
  {
    free_direct (inode, 12);
    free_indirect (inode, sectors - 12);
    free_doubly_indirect (inode, sectors - (12 + 128));
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;
  /* offset past EOF */
  if (offset >= data->length)
    return 0;
  
  /* don't read more bytes than the file has */
  size = data->length < size ? data->length : size;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          cache_read (sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
*/
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset)
{
  
  //printf ("\nbytes to write: %d\n", size);
  //printf ("bytes to write from: %d\n", offset);
  //printf ("initial size: %d\n", inode->data.length);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;

  if (inode->deny_write_cnt)
    return 0;
  
  bool do_not_change_length = false;
  if (offset + size <=  data->length)
    do_not_change_length = true;
    
  /* resize on write */
  bool resize_written = false;
  volatile int a = bytes_to_sectors (offset + size) == 0 ? 1 : bytes_to_sectors (offset + size);
  volatile int b = bytes_to_sectors (data->length) == 0 ? 1 : bytes_to_sectors (data->length);
  //printf ("a: %d\n", a);
  //printf ("b: %d\n", b);
  if (a > b)
    resize_written = resize_inode (inode, offset + size, data->length > 0);
    
  while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int chunk_size = size % BLOCK_SECTOR_SIZE == 0 ? BLOCK_SECTOR_SIZE : size;
      
      if (chunk_size > sector_left)
        chunk_size = sector_left;
      
      /* We need a bounce buffer. */
      if (bounce == NULL)
        {
          bounce = malloc (BLOCK_SECTOR_SIZE);
          if (bounce == NULL)
            break;
        }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
        cache_read (sector_idx, bounce);
      else
        memset (bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      cache_write (sector_idx, bounce);
      

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
      //printf ("size: %d\n", size);
      //printf ("offset: %d\n", offset);
      //printf ("bytes written: %d\n", bytes_written);
    }
  free (bounce);
  //printf ("total bytes written: %d\n", bytes_written);
  if (!resize_written && !do_not_change_length)
      data->length += bytes_written;
  
  //printf ("final size: %d\n\n", inode->data.length);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  char buff[BLOCK_SECTOR_SIZE];
  cache_read(inode->sector, buff);
  struct inode_disk *data = (struct inode_disk *) buff;
  return data->length;
}