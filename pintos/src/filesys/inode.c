#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <threads/synch.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

struct inode_disk* get_inode_disk(const struct inode* inode);

static bool inode_allocate(struct inode_disk* disk_inode, off_t length);
static bool inode_allocate_sector(block_sector_t* sector);
static bool inode_allocate_indirect(block_sector_t* sector, size_t count);
static bool inode_allocate_doubly_indirect(block_sector_t* sector, size_t count);
static void inode_deallocate(struct inode* inode);
static void inode_deallocate_indirect(block_sector_t sector, size_t count);
static void inode_deallocate_doubly_indirect(block_sector_t sector, size_t count);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t bytes_to_sectors(off_t size) { return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE); }

static inline size_t min(size_t a, size_t b) { return a < b ? a : b; }

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t byte_to_sector(const struct inode* inode, off_t pos) {
  ASSERT(inode != NULL);
  block_sector_t sector = -1;
  struct inode_disk* disk_inode = get_inode_disk(inode);

  if (pos < disk_inode->length) {
    off_t index = pos / BLOCK_SECTOR_SIZE;

    /* direct block */
    if (index < DIRECT_BLOCKS) {
      sector = disk_inode->direct_blocks[index];
      /* indirect block */
    } else if (index < DIRECT_BLOCKS + INDIRECT_BLOCKS) {
      /* normalize index for indirect_blocks */
      index -= DIRECT_BLOCKS;

      struct indirect_block_sector indirect_block;
      read_cache(disk_inode->indirect_block,
                 &indirect_block);
      sector = indirect_block.blocks[index];
      /* doubly indirect block */
    } else {
      /* normalize index for doubly_indirect_blocks */
      index -= (DIRECT_BLOCKS + INDIRECT_BLOCKS);
      struct indirect_block_sector indirect_block;
      int dib_index = index / INDIRECT_BLOCKS;
      int ib_index = index % INDIRECT_BLOCKS;

      read_cache(
          disk_inode->doubly_indirect_block,
          &indirect_block);
      read_cache(
          indirect_block.blocks[dib_index],
          &indirect_block);

      sector = indirect_block.blocks[ib_index];
    }
  }

  free(disk_inode);
  return sector;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void) { list_init(&open_inodes); }

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
  struct inode_disk* disk_inode = NULL;
  bool success = false;

  ASSERT(length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    if (inode_allocate(disk_inode, length)) {
      write_cache(sector, disk_inode);
      success = true;
    }
    free(disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode* inode_open(block_sector_t sector) {
  struct list_elem* e;
  struct inode* inode;

  /* Check whether this inode is already open. */
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes); e = list_next(e)) {
    inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector) {
      inode_reopen(inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->lock);
  block_read(fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode* inode_reopen(struct inode* inode) {
  if (inode != NULL) {
    inode->open_cnt++;
  }
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode* inode) { return inode->sector; }

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode* inode) {
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0) {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed) {
      free_map_release(inode->sector, 1);
      inode_deallocate(inode);
    }

    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode* inode) {
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode* inode, void* buffer_, off_t size, off_t offset) {
  uint8_t* buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t* bounce = NULL;

  while (size > 0) {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
      /* Read full sector directly into caller's buffer. */
#if CACHE_ENABLED == 0
      block_read(fs_device, sector_idx, buffer + bytes_read);
#else
      read_cache(sector_idx, buffer + bytes_read);
#endif
    } else {
      /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
      if (bounce == NULL) {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }
#if CACHE_ENABLED == 0
      block_read(fs_device, sector_idx, bounce);
#else
      read_cache(sector_idx, bounce);
#endif
      memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  free(bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode* inode, const void* buffer_, off_t size, off_t offset) {
  const uint8_t* buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t* bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (byte_to_sector(inode, offset + size - 1) == (unsigned int)-1) {
    /* Get inode_disk */
    struct inode_disk* disk_inode = get_inode_disk(inode);

    /* Allocate additional sectors for file */
    if (!inode_allocate(disk_inode, offset + size)) {
      free(disk_inode);
      return bytes_written;
    }

    /* Update inode_disk */
    disk_inode->length = offset + size;
    write_cache(inode_get_inumber(inode), disk_inode);
    free(disk_inode);
  }

  while (size > 0) {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE) {
      /* Write full sector directly to disk. */
#if CACHE_ENABLED == 0
      block_write(fs_device, sector_idx, buffer + bytes_written);
#else
      write_cache(sector_idx, (void*)(buffer + bytes_written));
#endif
    } else {
      /* We need a bounce buffer. */
      if (bounce == NULL) {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }

      /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
#if CACHE_ENABLED == 0
        block_read(fs_device, sector_idx, bounce);
#else
        read_cache(sector_idx, bounce);
#endif
      else
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
#if CACHE_ENABLED == 0
      block_write(fs_device, sector_idx, bounce);
#else
      write_cache(sector_idx, bounce);
#endif
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  free(bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode* inode) {
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode* inode) {
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode* inode) {
  ASSERT(inode != NULL);
  struct inode_disk* disk_inode = get_inode_disk(inode);
  off_t inode_len = disk_inode->length;
  free(disk_inode);

  return inode_len;
}

/* Acquire inode lock */
void inode_lock_acquire(struct inode* inode) { lock_acquire(&(inode->lock)); }

/* Release inode lock */
void inode_lock_release(struct inode* inode) { lock_release(&(inode->lock)); }

/* Read inode_disk from disk. */
struct inode_disk* get_inode_disk(const struct inode* inode) {
  ASSERT(inode != NULL);
  struct inode_disk* disk_inode = malloc(sizeof *disk_inode);
  read_cache(inode_get_inumber(inode),
             disk_inode);

  return disk_inode;
}

/* Allocate sectors in order of direct > indirect > doubly_indirect */
static bool inode_allocate(struct inode_disk* disk_inode, off_t length) {
  ASSERT(disk_inode != NULL);
  if (length < 0)
    return false;

  /* Calculate number of sectors needed */
  size_t i, offset, sectors_count = bytes_to_sectors(length);

  /* Allocate direct blocks needed */
  offset = min(sectors_count, DIRECT_BLOCKS);
  for (i = 0; i < offset; ++i) {
    if (!inode_allocate_sector(&disk_inode->direct_blocks[i])) {
      return false;
    }
  }
  sectors_count -= offset;

  if (sectors_count == 0)
    return true;

  /* Allocate indirect block */
  offset = min(sectors_count, INDIRECT_BLOCKS);
  if (!inode_allocate_indirect(&disk_inode->indirect_block, offset))
    return false;
  sectors_count -= offset;

  if (sectors_count == 0)
    return true;

  /* Allocate doubly indirect block */
  offset = min(sectors_count, INDIRECT_BLOCKS * INDIRECT_BLOCKS);
  if (!inode_allocate_doubly_indirect(&disk_inode->doubly_indirect_block, offset))
    return false;
  sectors_count -= offset;

  if (sectors_count == 0)
    return true;

  ASSERT(sectors_count == 0);
}

static bool inode_allocate_sector(block_sector_t* sector) {
  static char buffer[BLOCK_SECTOR_SIZE];
  if (!*sector) {
    if (!free_map_allocate(1, sector))
      return false;
    write_cache(*sector, buffer);
  }
  return true;
}

static bool inode_allocate_indirect(block_sector_t* sector, size_t count) {

  if (!inode_allocate_sector(sector)) {
    return false;
  }

  /* Read indirect block from cache */
  struct indirect_block_sector indirect_block;
  read_cache(*sector, &indirect_block);

  /* Allocate sectors according to count */
  for (size_t i = 0; i < count; i++) {
    if (!inode_allocate_sector(&indirect_block.blocks[i]))
      return false;
  }

  /* Write data on disk */
  write_cache(*sector, &indirect_block);

  return true;
}

static bool inode_allocate_doubly_indirect(block_sector_t* sector, size_t count) {
  if (!inode_allocate_sector(sector))
    return false;

  /* Read indirect block from cache */
  struct indirect_block_sector indirect_block;
  read_cache(*sector, &indirect_block);

  /* Allocate indirect blocks according to count */
  size_t sectors_count;
  size_t limit = DIV_ROUND_UP(count, INDIRECT_BLOCKS);
  for (size_t i = 0; i < limit; ++i) {
    sectors_count = min(count, INDIRECT_BLOCKS);
    if (!inode_allocate_indirect(&indirect_block.blocks[i], sectors_count))
      return false;
    count -= sectors_count;
  }

  /* Write data on disk */
  write_cache(*sector, &indirect_block);

  return true;
}

static void inode_deallocate(struct inode* inode) {

  ASSERT(inode != NULL);

  /* Get inode_disk from inode */
  struct inode_disk* disk_inode = get_inode_disk(inode);
  off_t len = disk_inode->length;
  if (len < 0)
    return;

  /* Calculate number of sectors */
  size_t offset, sectors_count = bytes_to_sectors(len);

  /* Deallocate direct blocks */
  offset = min(sectors_count, DIRECT_BLOCKS);
  for (size_t i = 0; i < offset; i++) {
    free_map_release(disk_inode->direct_blocks[i], 1);
  }
  sectors_count -= offset;

  if (sectors_count == 0) {
    free(disk_inode);
    return;
  }

  /* Deallocate indirect blocks */
  offset = min(sectors_count, INDIRECT_BLOCKS);
  inode_deallocate_indirect(disk_inode->indirect_block, offset);
  sectors_count -= offset;

  if (sectors_count == 0) {
    free(disk_inode);
    return;
  }

  /* Deallocate doubly indirect block */
  offset = min(sectors_count, INDIRECT_BLOCKS * INDIRECT_BLOCKS);
  inode_deallocate_doubly_indirect(disk_inode->doubly_indirect_block, offset);
  sectors_count -= offset;

  free(disk_inode);
  ASSERT(sectors_count == 0);
}

static void inode_deallocate_indirect(block_sector_t sector, size_t count) {
  struct indirect_block_sector indirect_block;
  read_cache(sector, &indirect_block);

  for (size_t i = 0; i < count; i++) {
    free_map_release(indirect_block.blocks[i], 1);
  }
  free_map_release(sector, 1);
}

static void inode_deallocate_doubly_indirect(block_sector_t sector, size_t count) {
  struct indirect_block_sector indirect_block;

  read_cache(sector, &indirect_block);

  size_t sectors_count, offset = DIV_ROUND_UP(count, INDIRECT_BLOCKS);
  for (size_t i = 0; i < offset; i++) {
    sectors_count = min(count, INDIRECT_BLOCKS);
    inode_deallocate_indirect(indirect_block.blocks[i], sectors_count);
    count -= sectors_count;
  }

  free_map_release(sector, 1);
}
