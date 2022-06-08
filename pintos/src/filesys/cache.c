#include "cache.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <hash.h>
#include <debug.h>

#define MAX_CACHE_SIZE 64

struct cache_entry {
  struct hash_elem hash_elem;   /* Hash element for hash table. */
  block_sector_t sector;        /* Sector number in disk. */
  char data[BLOCK_SECTOR_SIZE]; /* Data in sector. */
  uint32_t last_access;         /* Last access time (measured by time variable
                                   above, not real time). */
  struct lock lock;             /* Only one process accesses the cache at any
                                   time. */
};

/* Used to keep track of last access time. We're implementing LRU. 
   It is increased with every access to cache. */
static uint32_t access_count;

static struct hash cache;

/* Each process gets this lock when tries to create a new entry in cache. */
static struct lock cache_lock;

static void cache_entry_destroy(struct hash_elem* e, void* aux UNUSED);
static unsigned cache_entry_hash(const struct hash_elem* p_, void* aux UNUSED);
static bool cache_entry_less(const struct hash_elem* a_, const struct hash_elem* b_,
                             void* aux UNUSED);
static struct cache_entry* cache_lookup(block_sector_t sector);
static struct cache_entry* remove_lru(void);
static struct cache_entry* create_entry(block_sector_t sector);

/* Reads the content written at SECTOR in disk and writes it to BUFFER. If
   availabe in cache, returns it immediately, otherwise fetches the sector
   from the disk into cache first. */
void read_cache(block_sector_t sector, void* buffer) {
#if CACHE_ENABLED == 1
  lock_acquire(&cache_lock);
  struct cache_entry* cache_entry = cache_lookup(sector);
  access_count++;
  if (cache_entry == NULL) {
    /* Cache is not available. We have to load from disk. */
    /* Create a cache_entry for SECTOR. */
    cache_entry = create_entry(sector);
    lock_acquire(&cache_entry->lock);
    if (hash_size(&cache) > MAX_CACHE_SIZE) {
      /* Cache is full. Remove the LRU from cache's hash table. Waiters have
         their own pointer to that entry. Remove it so that new threads can't
         find this entry anymore. Then release the lock, wait for your turn on
         that entry and write to disk and free its content. */
      struct cache_entry* lru = remove_lru();
      lock_release(&cache_lock);
      lock_acquire(&lru->lock);
      block_write(fs_device, lru->sector, lru->data);
      free(lru);
    } else {
      lock_release(&cache_lock);
    }
    /* Read data form disk and load it in cache. */
    uint8_t _buffer[BLOCK_SECTOR_SIZE];
    block_read(fs_device, sector, _buffer);
    memcpy(cache_entry->data, _buffer, BLOCK_SECTOR_SIZE);
  } else {
    /* Cache is available. */
    cache_entry->last_access = access_count;
    lock_acquire(&cache_entry->lock);
    lock_release(&cache_lock);
  }
  /* Read and return from cache. */
  memcpy(buffer, cache_entry->data, BLOCK_SECTOR_SIZE);
  lock_release(&cache_entry->lock);
#else
  block_read(fs_device, sector, buffer);
#endif
}

/* Writes the content in BUFFER to cache which later is written to SECTOR in
   disk. BUFFER must be at least BLOCK_SECTOR_SIZE long. If the cache is full
   the LRU sector is written to disk first. */
void write_cache(block_sector_t sector, void* buffer) {
#if CACHE_ENABLED == 1
  lock_acquire(&cache_lock);
  struct cache_entry* cache_entry = cache_lookup(sector);
  access_count++;
  if (cache_entry == NULL) {
    /* Cache is not available. Create a cache_entry for SECTOR. */
    cache_entry = create_entry(sector);
    lock_acquire(&cache_entry->lock);
    if (hash_size(&cache) > MAX_CACHE_SIZE) {
      /* Cache is full. Remove the LRU from cache's hash table. Waiters have
         their own pointer to that entry. Remove it so that new threads can't
         find this entry anymore. Then release the lock, wait for your turn on
         that entry and free its content. */
      struct cache_entry* lru = remove_lru();
      lock_release(&cache_lock);
      lock_acquire(&lru->lock);
      block_write(fs_device, lru->sector, lru->data);
      free(lru);
    } else {
      lock_release(&cache_lock);
    }
  } else {
    /* Cache is available. */
    cache_entry->last_access = access_count;
    lock_acquire(&cache_entry->lock);
    lock_release(&cache_lock);
  }
  /* Read data form buffer and load it in cache. */
  memcpy(cache_entry->data, buffer, BLOCK_SECTOR_SIZE);
  lock_release(&cache_entry->lock);
#else
  block_write(fs_device, sector, buffer);
#endif
}

/* Writes all cache content to disk. Deallocates the memory used by cache. */
void cache_done() {
  struct hash_iterator i;
  struct cache_entry* e;
  hash_first(&i, &cache);
  struct hash_elem* cur = hash_next(&i);
  while ((cur = hash_next(&i))) {
    e = hash_entry(cur, struct cache_entry, hash_elem);
    block_write(fs_device, e->sector, e->data);
  }
  hash_destroy(&cache, cache_entry_destroy);
}

/* Initialize the hash table for cache. */
void cache_init() {
  lock_init(&cache_lock);
  hash_init(&cache, cache_entry_hash, cache_entry_less, NULL);
}

/* Finds and removes the LRU sector in cache. Note that this does not
   deallocate the memory; It merely removes the entry from cache's hash_table.
   */
static struct cache_entry* remove_lru() {
  struct hash_iterator i;
  struct cache_entry* oldest_elem = NULL;
  struct cache_entry* now_elem;

  hash_first(&i, &cache);
  struct hash_elem* cur = hash_next(&i);
  while ((cur = hash_next(&i))) {
    now_elem = hash_entry(cur, struct cache_entry, hash_elem);
    if (oldest_elem == NULL || now_elem->last_access < oldest_elem->last_access)
      oldest_elem = now_elem;
  }
  ASSERT(oldest_elem != NULL);
  hash_delete(&cache, &oldest_elem->hash_elem);
  return oldest_elem;
}

/* Frees a cache entry. This is used by hash_destroy(). */
static void cache_entry_destroy(struct hash_elem* e, void* aux UNUSED) {
  struct cache_entry* c = hash_entry(e, struct cache_entry, hash_elem);
  free(c);
}

/* Returns a hash value for cache_entry p. */
static unsigned cache_entry_hash(const struct hash_elem* p_, void* aux UNUSED) {
  const struct cache_entry* p = hash_entry(p_, struct cache_entry, hash_elem);
  return hash_int(p->sector);
}

/* Returns true if cache_entry a precedes cache_entry b. */
static bool cache_entry_less(const struct hash_elem* a_, const struct hash_elem* b_,
                             void* aux UNUSED) {
  const struct cache_entry* a = hash_entry(a_, struct cache_entry, hash_elem);
  const struct cache_entry* b = hash_entry(b_, struct cache_entry, hash_elem);
  return a->sector < b->sector;
}

/* Returns cache_entry containing SECTOR or null if no such cache_entry
   exists. */
static struct cache_entry* cache_lookup(block_sector_t sector) {
  struct cache_entry c;
  struct hash_elem* e;
  c.sector = sector;
  e = hash_find(&cache, &c.hash_elem);
  return e != NULL ? hash_entry(e, struct cache_entry, hash_elem) : NULL;
}

/* Creates an empty cache_entry with sector = SECOTR and inserts it into the
   cache's hash table. */
static struct cache_entry* create_entry(block_sector_t sector) {
  struct cache_entry* cache_entry = malloc(sizeof *cache_entry);
  cache_entry->last_access = access_count;
  cache_entry->sector = sector;
  lock_init(&cache_entry->lock);
  struct hash_elem* ret = hash_insert(&cache, &cache_entry->hash_elem);
  ASSERT(ret == NULL);
  return cache_entry;
}
