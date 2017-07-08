#include <filesys/cache.h>
#include <filesys/filesys.h>
#include <threads/synch.h>
#include <stdbool.h>
#include <string.h>
#include <list.h>

struct cache_block 
{
  block_sector_t sector_tag; // Which sector is currently in the cache
  char sector[BLOCK_SECTOR_SIZE]; // Basically 512 contiguous bytes.
  struct lock block_lock; // One Lock for each block.
  bool dirty_bit; // Does it need to be written to disk on eviction?
  struct list_elem elem; // Will go into LRU list.
};

static struct cache_block blocks[CACHE_SIZE]; // Our cache blocks.

/* ------------------------------------------
   | MRU   | ......................| LRU    |
   |  0    |                       |   N    |
   ------------------------------------------
*/
static struct list LRU_list; // Doubly Linked List to implement LRU.
static struct lock LRU_lock; // Only one thread can modify list at a time.

/* Simply initializes all the locks in the cache.
   We don't need to add elems to LRU_list yet. */
void cache_init (void) 
{
  list_init (&LRU_list);
  lock_init (&LRU_lock);
  int i = 0;
  struct cache_block *block;
  for (; i < CACHE_SIZE; i += 1) 
  {
    block = &blocks[i];
    lock_init (&block->block_lock);
  }
}

/* Reads sector into the buffer from the cache. */
void cache_read (block_sector_t sector, void *buffer) 
{
  lock_acquire (&LRU_lock);
  int size = list_size (&LRU_list);
  int i = 0;
  struct cache_block *block;
  // Check if the block we want is in the cache
  for (; i < size; i += 1) 
  {
    block = &blocks[i];
    if (block->sector_tag == sector) 
    {
      list_remove (&block->elem);
      list_push_front (&LRU_list, &block->elem);
      lock_acquire (&block->block_lock);
      lock_release (&LRU_lock);
      memcpy (buffer, block->sector, BLOCK_SECTOR_SIZE);
      lock_release (&block->block_lock);
      return ;
    }
  }
  // LRU_list not full. Add sector to next empty block.
  if (size < 64) 
  {
    block = &blocks[i];
    block->dirty_bit = false;
    block->sector_tag = sector;
    list_push_front (&LRU_list, &block->elem);
    lock_acquire (&block->block_lock);
    lock_release (&LRU_lock);
    block_read (fs_device, sector, block->sector);
    memcpy (buffer, block->sector, BLOCK_SECTOR_SIZE);
    lock_release (&block->block_lock);
    return ; 
  }
  else {
    // Eviction
    block = list_entry (list_pop_back (&LRU_list), struct cache_block, elem);
    list_push_front (&LRU_list, &block->elem);
    lock_acquire (&block->block_lock);
    block_sector_t old_sector = block->sector_tag;
    block->sector_tag = sector;
    lock_release (&LRU_lock);
    if (block->dirty_bit) 
    {
      block_write (fs_device, old_sector, block->sector);
      block->dirty_bit = false;
    }
    block_read (fs_device, sector, block->sector);
    memcpy (buffer, block->sector, BLOCK_SECTOR_SIZE);
    lock_release (&block->block_lock);
    return ;
  }
}

/* Writes buffer contents into sector in the cache. */
void cache_write (block_sector_t sector, const void *buffer) 
{
  lock_acquire (&LRU_lock);
  int size = list_size (&LRU_list);
  int i = 0;
  struct cache_block *block;
  // Check if the block we want is in the cache
  for (; i < size; i += 1) 
  {
    block = &blocks[i];
    if (block->sector_tag == sector) 
    {
      list_remove (&block->elem);
      list_push_front (&LRU_list, &block->elem);
      lock_acquire (&block->block_lock);
      lock_release (&LRU_lock);
      block->dirty_bit = true;
      memcpy (block->sector, buffer, BLOCK_SECTOR_SIZE);
      lock_release (&block->block_lock);
      return ;
    }
  }
  // LRU_list not full. Add sector to next empty block.
  if (size < 64) 
  {
    block = &blocks[i];
    block->dirty_bit = true;
    block->sector_tag = sector;
    list_push_front (&LRU_list, &block->elem);
    lock_acquire (&block->block_lock);
    lock_release (&LRU_lock);
    memcpy (block->sector, buffer, BLOCK_SECTOR_SIZE);
    lock_release (&block->block_lock);
    return ; 
  }
  else {
    // Eviction
    block = list_entry(list_pop_back (&LRU_list), struct cache_block, elem);
    list_push_front (&LRU_list, &block->elem);
    lock_acquire (&block->block_lock);
    block_sector_t old_sector = block->sector_tag;
    block->sector_tag = sector;
    lock_release (&LRU_lock);
    if (block->dirty_bit) 
    {
      block_write (fs_device, old_sector, block->sector);
    }
    block->dirty_bit = true;
    memcpy (block->sector, buffer, BLOCK_SECTOR_SIZE);
    lock_release (&block->block_lock);
    return ;
  }
}

/* Flushes all the dirty blocks to disk */
void cache_flush (void) 
{
  lock_acquire (&LRU_lock);
  int size = list_size (&LRU_list);
  int i = 0;
  struct cache_block *block;
  for (; i < size; i += 1) 
  {
    block = &blocks[i];
    if (block->dirty_bit) 
    {
      lock_acquire (&block->block_lock);
      block_write (fs_device, block->sector_tag, block->sector);
      block->dirty_bit = false;
      lock_release (&block->block_lock);
    }
  }
  lock_release (&LRU_lock);
}
