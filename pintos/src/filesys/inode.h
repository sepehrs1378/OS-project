#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include <lib/kernel/list.h>
#include "devices/block.h"
#include "threads/synch.h"

#define DIRECT_BLOCKS 124
#define INDIRECT_BLOCKS 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk {
  block_sector_t direct_blocks[DIRECT_BLOCKS]; /* Direct blocks array */
  block_sector_t indirect_block;               /* indirect block address */
  block_sector_t doubly_indirect_block;        /* doubly indirect block address */

  off_t length;   /* File size in bytes. */
  unsigned magic; /* Magic number. */
};

/* Indirect-block structure */
struct indirect_block_sector {
  block_sector_t blocks[INDIRECT_BLOCKS];
};

/* In-memory inode. */
struct inode {
  struct lock lock;       /* Used to synch modifing the dir/file which owns this inode. */
  struct list_elem elem;  /* Element in inode list. */
  block_sector_t sector;  /* Sector number of disk location. */
  int open_cnt;           /* Number of openers. */
  bool removed;           /* True if deleted, false otherwise. */
  int deny_write_cnt;     /* 0: writes ok, >0: deny writes. */
  struct inode_disk data; /* Inode content. */
};

struct bitmap;

void inode_init(void);
bool inode_create(block_sector_t, off_t);
struct inode* inode_open(block_sector_t);
struct inode* inode_reopen(struct inode*);
block_sector_t inode_get_inumber(const struct inode*);
void inode_close(struct inode*);
void inode_remove(struct inode*);
void inode_lock_acquire(struct inode* inode);
void inode_lock_release(struct inode* inode);
off_t inode_read_at(struct inode*, void*, off_t size, off_t offset);
off_t inode_write_at(struct inode*, const void*, off_t size, off_t offset);
void inode_deny_write(struct inode*);
void inode_allow_write(struct inode*);
off_t inode_length(const struct inode*);

#endif /* filesys/inode.h */
