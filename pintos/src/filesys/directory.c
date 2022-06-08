#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/free-map.h"

/* A directory. */
struct dir {
  struct inode* inode; /* Backing store. */
  off_t pos;           /* Current position. */
};

/* A single directory entry. */
struct dir_entry {
  block_sector_t inode_sector; /* Sector number of header. */
  char name[NAME_MAX + 1];     /* Null terminated file name. */
  bool in_use;                 /* In use or free? */
  bool is_file;                /* file or dir? */
};

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt) {
  return inode_create(sector, entry_cnt * sizeof(struct dir_entry));
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir* dir_open(struct inode* inode) {
  struct dir* dir = calloc(1, sizeof *dir);
  if (inode != NULL && dir != NULL) {
    dir->inode = inode;
    dir->pos = 0;
    return dir;
  } else {
    inode_close(inode);
    free(dir);
    return NULL;
  }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir* dir_open_root(void) {
  return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir* dir_reopen(struct dir* dir) {
  return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void dir_close(struct dir* dir) {
  if (dir != NULL) {
    inode_close(dir->inode);
    free(dir);
  }
}

/* Returns the inode encapsulated by DIR. */
struct inode* dir_get_inode(struct dir* dir) {
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir* dir, const char* name, struct dir_entry* ep, off_t* ofsp) {
  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
    if (e.in_use && !strcmp(name, e.name)) {
      if (ep != NULL)
        *ep = e;
      if (ofsp != NULL)
        *ofsp = ofs;
      return true;
    }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns 1 if dir exists, 2 if file exists, 0 otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
int dir_lookup(const struct dir* dir, const char* name, struct inode** inode) {
  struct dir_entry e;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  if (lookup(dir, name, &e, NULL))
    *inode = inode_open(e.inode_sector);
  else
    *inode = NULL;

  if (!*inode)
    return 0;
  else
    return e.is_file ? 2 : 1;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool dir_add(struct dir* dir, const char* name, bool is_file, block_sector_t inode_sector) {
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen(name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup(dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.is_file = is_file;
  e.in_use = true;
  strlcpy(e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir* dir, const char* name) {
  struct dir_entry e;
  struct inode* inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  inode_lock_acquire(dir->inode);

  /* Find directory entry. */
  if (!lookup(dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove(inode);
  success = true;

done:
  inode_lock_release(dir->inode);

  inode_close(inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool dir_readdir(struct dir* dir, char name[NAME_MAX + 1]) {
  struct dir_entry e;

  while (inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
    dir->pos += sizeof e;
    if (e.in_use) {
      strlcpy(name, e.name, NAME_MAX + 1);
      return true;
    }
  }
  return false;
}

/* Creates a new dir inside CONTAINER_DIR, named NAME. Returns true on success and false otherwise. */
bool dir_mkdir(struct dir* container_dir, char name[NAME_MAX + 1]) {
  block_sector_t inode_sector = 0;

  inode_lock_acquire(container_dir->inode);

  bool success = (container_dir != NULL && free_map_allocate(1, &inode_sector) &&
                  inode_create(inode_sector, 16 * sizeof(struct dir_entry)) &&
                  dir_add(container_dir, name, false, inode_sector));
  if (!success && inode_sector != 0) {
    free_map_release(inode_sector, 1);
    return success;
  }

  struct dir* made_dir = dir_open(inode_open(inode_sector));
  dir_add(made_dir, ".", false, inode_sector);
  dir_add(made_dir, "..", false, container_dir->inode->sector);

  inode_lock_release(container_dir->inode);

  return success;
}

bool dir_is_empty(struct dir* dir) {
  char* temp_name = malloc(NAME_MAX + 1);
  while (true) {
    bool found = dir_readdir(dir, temp_name);
    if (found) {
      if (!strcmp(temp_name, ".") || !strcmp(temp_name, ".."))
        continue;
      else {
        free(temp_name);
        return false;
      }
    } else {
      free(temp_name);
      return true;
    }
  }
}

/* Extracts a file name part from *SRCP into PART, and updates *SRCP so that the
   next call will return the next file name part. Returns 1 if successful, 0 at
   end of string, -1 for a too-long file name part. */
int get_next_part(char part[NAME_MAX + 1], const char** srcp) {
  const char* src = *srcp;
  char* dst = part;

  /* Skip leading slashes. If it's all slashes, we're done. */
  while (*src == '/')
    src++;
  if (*src == '\0')
    return 0;

  /* Copy up to NAME_MAX character from SRC to DST. Add null terminator. */
  while (*src != '/' && *src != '\0') {
    if (dst < part + NAME_MAX)
      *dst++ = *src;
    else
      return -1;
    src++;
  }
  *dst = '\0';

  /* Advance source pointer. */
  *srcp = src;
  return 1;
}

/* If PATH is absolute returns itself, if it is relative returns concat of cwd & path. */
char* convert_to_abs_path(char* path) {
  struct thread* t = thread_current();

  if (path[0] == '/') { /* Path is absolute. */
    char* _path = malloc(strlen(path) + 1);
    memcpy(_path, path, strlen(path) + 1);
    return _path;
  } else { /* Path is relative. */
    char* _path = malloc(strlen(path) + strlen(t->cwd) + 1);
    memcpy(_path, t->cwd, strlen(t->cwd));
    memcpy(_path + strlen(t->cwd), path, strlen(path) + 1);
    return _path;
  }
}

/* Simplifies a path and removes . and .. from it.
   Returns true on success and false if there's a long name in path. */
bool simplify_path(char* path) {
  char* _path = path;
  char* part = malloc(NAME_MAX + 1);
  char* simple_path = malloc(strlen(path) + 1);
  simple_path[0] = '/';
  simple_path[1] = '\0';

  bool success = true;
  while (true) {
    int res = get_next_part(part, &path);
    if (res == -1) {
      success = false;
      break;
    } else if (res == 0) {
      success = true;
      break;
    } else {
      if (!strcmp(part, ".")) {
        //empty
      } else if (!strcmp(part, "..")) {
        go_to_parent_in_path(simple_path);
      } else {
        add_dir_to_path(simple_path, part);
      }
    }
  }

  if (success) {
    memcpy(_path, simple_path, strlen(simple_path) + 1);
  }
  free(part);
  free(simple_path);
  return success;
}

/* Adds DIR_NAME to the end of string PATH, e.g. /a/b/ with c becomes /a/b/c/. */
void add_dir_to_path(char* path, char* dir_name) {
  int path_len = strlen(path);
  int dir_name_len = strlen(dir_name);
  memcpy(path + path_len, dir_name, dir_name_len + 1);
  path_len = strlen(path);
  path[path_len] = '/';
  path[path_len + 1] = '\0';
}

/* Removes the last dir from PATH. 
   Will do nothing if path is "/" and return false, otherwise will return true. */
bool go_to_parent_in_path(char* path) {
  int path_len = strlen(path);

  if (path_len == 1) /* Path is "/". */
    return false;

  char* _path = path;
  _path += path_len - 1;

  while (*_path == '/')
    _path -= 1;
  while (*_path != '/')
    _path -= 1;
  *(_path + 1) = '\0';

  return true;
}

/* Sets *CONTENT_NAME to the name of last element in PATH. */
void get_content_name(char* path, char** content_name) {
  char* _path = path + strlen(path) - 1;
  char* end;
  char* start;

  while (*_path == '/')
    _path -= 1;
  end = _path;
  while (*_path != '/')
    _path -= 1;
  start = _path + 1;

  memcpy(*content_name, start, end - start + 1);
  (*content_name)[end - start + 1] = '\0';
}

/* Iterates PATH and sets *CONTENT_NAME to the last dir/file of it.
   Also sets IS_FILE is found content is file or dir accordingly.
   Returns true on success and false if there's a long name in PATH or the PATH doesn't exist in filesys. */
bool get_content(char* path, bool* is_file, void** content) {
  struct dir* cur_dir = dir_open_root();
  char* part = malloc(NAME_MAX + 1);
  struct inode* inode;
  inode = cur_dir->inode;
  *is_file = false;
  *content = cur_dir;

  int res;
  int stat;
  bool file_in_path = false;
  while (true) {
    res = get_next_part(part, &path);
    if (res == -1) {
      free(part);
      return false;
    } else if (res == 0) {
      free(part);
      return true;
    } else if (res == 1) {
      if (file_in_path) { /* Two files in path, this is impossible. */
        free(part);
        return false;
      }
      stat = dir_lookup(cur_dir, part, &inode);
      if (stat == 0) { /* Nothing found. */
        return false;
      } else if (stat == 1) { /* Dir found. */
        *is_file = false;
        cur_dir = dir_open(inode);
        *content = (struct dir*)dir_open(inode);
      } else if (stat == 2) { /* File found. */
        file_in_path = true;
        *is_file = true;
        *content = (struct file*)file_open(inode);
      } else {
        ASSERT(1 == 2);
      }
    } else {
      ASSERT(1 == 2);
    }
  }
}

/*  */
bool is_path_valid(char* path, bool last_exists) {
  void* content;
  bool is_file;
  if (last_exists) {
    bool stat = get_content(path, &is_file, &content);
    return stat;
  } else {
    char* _path = malloc(strlen(path) + 1);
    memcpy(_path, path, strlen(path) + 1);
    go_to_parent_in_path(_path);
    bool stat = get_content(_path, &is_file, &content);
    free(_path);
    return stat;
  }
}

/* Sets DIR to the directory of the last element in PATH.
   Returns true on success and false if the directory doesn't exist or PATH is not correct. */
bool get_container_dir(char* path, struct dir** dir) {
  char* _path = malloc(strlen(path) + 1);
  memcpy(_path, path, strlen(path) + 1);
  go_to_parent_in_path(_path);
  bool is_file;
  void* content;
  bool stat = get_content(_path, &is_file, &content);

  if (!stat || is_file) {
    free(_path);
    return false;
  }

  *dir = (struct dir*)content;
  free(_path);
  return true;
}
