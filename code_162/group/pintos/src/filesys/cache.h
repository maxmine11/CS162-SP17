#include "devices/block.h"

/* Max number of sectors in our buffer cache. */
#define CACHE_SIZE 64

void cache_init (void);
void cache_read (block_sector_t sector, void * buffer);
void cache_write (block_sector_t sector, const void * buffer);
void cache_flush (void);
