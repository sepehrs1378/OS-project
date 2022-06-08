#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

#define CACHE_ENABLED 1

void cache_init(void);
void read_cache(block_sector_t sector, void* buffer);
void write_cache(block_sector_t sector, void* buffer);
void cache_done(void);

#endif /* filesys/cache.h */
