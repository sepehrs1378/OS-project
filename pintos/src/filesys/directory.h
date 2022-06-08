#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;

/* Opening and closing directories. */
bool dir_create(block_sector_t sector, size_t entry_cnt);
struct dir* dir_open(struct inode*);
struct dir* dir_open_root(void);
struct dir* dir_reopen(struct dir*);
void dir_close(struct dir*);
struct inode* dir_get_inode(struct dir*);

/* Reading and writing. */
int dir_lookup(const struct dir*, const char* name, struct inode**);
bool dir_add(struct dir*, const char* name, bool is_file, block_sector_t);
bool dir_remove(struct dir*, const char* name);
bool dir_readdir(struct dir*, char name[NAME_MAX + 1]);

bool dir_mkdir(struct dir* dir, char name[NAME_MAX + 1]);
bool dir_is_empty(struct dir* dir);

char* convert_to_abs_path(char*);
bool simplify_path(char* path);
int get_next_part(char part[NAME_MAX + 1], const char**);
void add_dir_to_path(char* path, char* dir_name);
bool go_to_parent_in_path(char* path);
void get_content_name(char* path, char** content_name);
bool get_content(char* path, bool* is_file, void** content);
bool is_path_valid(char* path, bool last_exists);
bool get_container_dir(char* path, struct dir** dir);

#endif /* filesys/directory.h */
