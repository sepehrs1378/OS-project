#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <threads/malloc.h>
#include <lib/kernel/console.h>
#include "pagedir.h"
#include <filesys/file.h>
#include <filesys/filesys.h>
#include <lib/kernel/list.h>
#include <devices/input.h>
#include <devices/shutdown.h>
#include <devices/block.h>
#include <userprog/process.h>
#include <filesys/directory.h>
#include <filesys/free-map.h>
#include <filesys/inode.h>

static void syscall_handler(struct intr_frame*);

void terminate(struct intr_frame*, uint32_t);

void handle_exit(struct intr_frame*, uint32_t*);
void handle_exec(struct intr_frame*, uint32_t*);
void handle_wait(struct intr_frame*, uint32_t*);
void handle_practice(struct intr_frame*, uint32_t*);
void handle_create(struct intr_frame*, uint32_t*);
void handle_remove(struct intr_frame*, uint32_t*);
void handle_open(struct intr_frame*, uint32_t*);
void handle_close(struct intr_frame*, uint32_t*);
void handle_read(struct intr_frame*, uint32_t*);
void handle_write(struct intr_frame*, uint32_t*);
void handle_seek(struct intr_frame*, uint32_t*);
void handle_tell(struct intr_frame*, uint32_t*);
void handle_filesize(struct intr_frame*, uint32_t*);
void handle_chdir(struct intr_frame* f, uint32_t* args);
void handle_mkdir(struct intr_frame* f, uint32_t* args);
void handle_readdir(struct intr_frame* f, uint32_t* args);
void handle_isdir(struct intr_frame* f, uint32_t* args);
void handle_inumber(struct intr_frame* f, uint32_t* args);

void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = ((uint32_t*)f->esp);
  /* We have used the second method described in the project reference.
       First we check if the value we're dereferencing is a valid address,
       (i.e. below PHYS_BASE) and then we handle the error in
       page fault handler.
       For more information see Project1 - page 12, Accessing User Memory. */
  validate(args, sizeof(uint32_t));
  int type = args[0];

  /*
     * The following print statement, if uncommented, will print out the syscall
     * number whenever a process enters a system call. You might find it useful
     * when debugging. It will cause tests to fail, however, so you should not
     * include it in your final submission.
     */

  switch (type) {
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      handle_exit(f, args);
      break;

    case SYS_EXEC:
      handle_exec(f, args);
      break;

    case SYS_WAIT:
      handle_wait(f, args);
      break;

    case SYS_CREATE:
      handle_create(f, args);
      break;

    case SYS_REMOVE:
      handle_remove(f, args);
      break;

    case SYS_OPEN:
      handle_open(f, args);
      break;

    case SYS_FILESIZE:
      handle_filesize(f, args);
      break;

    case SYS_READ:
      handle_read(f, args);
      break;

    case SYS_WRITE:
      handle_write(f, args);
      break;

    case SYS_SEEK:
      handle_seek(f, args);
      break;

    case SYS_TELL:
      handle_tell(f, args);
      break;

    case SYS_CLOSE:
      handle_close(f, args);
      break;

    case SYS_PRACTICE:
      handle_practice(f, args);
      break;

    case SYS_CHDIR:
      handle_chdir(f, args);
      break;

    case SYS_MKDIR:
      handle_mkdir(f, args);
      break;

    case SYS_READDIR:
      handle_readdir(f, args);
      break;

    case SYS_ISDIR:
      handle_isdir(f, args);
      break;

    case SYS_INUMBER:
      handle_inumber(f, args);
      break;

    case SYS_DISK_R:
      f->eax = block_read_cnt(fs_device);
      break;

    case SYS_DISK_W:
      f->eax = block_write_cnt(fs_device);
      break;

    default:
      ASSERT(1 == 2);
  }
}

/* Terminates the program with EXIT_CODE. Sets EXIT_CODE in F->eax as return value. */
void terminate(struct intr_frame* f, uint32_t exit_code) {
  f->eax = exit_code;
  printf("%s: exit(%d)\n", thread_current()->name, exit_code);
  thread_exit(exit_code);
}

/* Handle exit syscall. */
void handle_exit(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(uint32_t));
  terminate(f, args[1]);
}

/* Handle exec syscall. */
void handle_exec(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(const char*));
  const char* cmd_line = (char*)args[1];
  validate_string(cmd_line, 256);
  tid_t tid = process_execute(cmd_line);
  f->eax = tid;
}

/* Handle wait syscall. */
void handle_wait(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(tid_t));
  tid_t tid = args[1];
  int exit_code = process_wait(tid);
  f->eax = exit_code;
}

/* Handle create syscall. */
void handle_create(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(char*) + sizeof(unsigned));
  char* path = (char*)args[1];
  unsigned size = args[2];
  if (!path) {
    terminate(f, -1);
  }
  validate_string(path, 1024);
  if (!strcmp(path, "")) {
    terminate(f, -1);
  }
  char* _path = convert_to_abs_path(path);
  bool valid = is_path_valid(_path, false);
  bool not_long = simplify_path(_path);
  if (!not_long || !valid) {
    free(_path);
    f->eax = 0;
    return;
  }

  struct dir* container_dir;
  char* content_name = malloc(NAME_MAX + 1);
  bool stat = get_container_dir(_path, &container_dir);
  get_content_name(_path, &content_name);
  if (!stat) {
    f->eax = 0;
  } else {
    bool created = file_create(content_name, size, container_dir);
    if (created) {
      f->eax = 1;
    } else {
      f->eax = 0;
    }
  }

  free(_path);
  free(content_name);
  dir_close(container_dir);
}

/* Handle remove syscall. */
void handle_remove(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(char*));
  char* path = (char*)args[1];
  validate_string(path, 1024);
  char* _path = convert_to_abs_path(path);
  bool valid = is_path_valid(_path, true);
  bool not_long = simplify_path(_path);
  if (!not_long || !valid || !strcmp(_path, "/")) {
    free(_path);
    f->eax = 0;
    return;
  }

  bool is_file;
  void* content;
  bool stat = get_content(_path, &is_file, &content);
  if (!stat) {
    f->eax = 0;
  } else {
    struct dir* container_dir;
    char* content_name = malloc(NAME_MAX + 1);

    if (is_file) {
      get_content_name(_path, &content_name);
      get_container_dir(_path, &container_dir);
      bool removed = dir_remove(container_dir, content_name);
      f->eax = removed ? 1 : 0;
      dir_close(container_dir);
    } else {
      struct dir* dir = content;
      char* temp_name = malloc(NAME_MAX + 1);

      if (dir_is_empty(dir)) { /* Dir empty. */
        get_content_name(_path, &content_name);
        get_container_dir(_path, &container_dir);
        bool removed = dir_remove(container_dir, content_name);
        f->eax = removed ? 1 : 0;
        dir_close(container_dir);
      } else { /* Dir not empty. */
        f->eax = 0;
      }

      free(temp_name);
    }

    free(content_name);
  }

  free(_path);
}

/* Handle open syscall. */
void handle_open(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(char*));
  char* path = (char*)args[1];
  validate_string(path, 1024);
  if (!path || !strcmp(path, "")) {
    f->eax = -1;
    return;
  }
  char* _path = convert_to_abs_path(path);
  bool valid = is_path_valid(_path, true);
  bool not_long = simplify_path(_path);
  if (!not_long || !valid) {
    free(_path);
    f->eax = -1;
    return;
  }

  bool is_file = false;
  void* content;
  bool stat = get_content(_path, &is_file, &content);
  if (!stat) {
    f->eax = -1;
  } else {
    struct fd* fd = malloc(sizeof(struct fd));
    struct thread* t = thread_current();
    if (is_file) {
      struct file* file = (struct file*)content;
      fd->content = file;
      fd->is_file = true;
    } else {
      struct dir* dir = (struct dir*)content;
      fd->content = dir;
      fd->is_file = false;
    }
    fd->fd_id = t->next_fd_id++;
    hash_insert(&t->fd_table, &fd->hash_elem);

    f->eax = fd->fd_id;
  }
}

/* Handle filesize syscall. */
void handle_filesize(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int));
  int fd_id = args[1];

  struct file* file = fd_file_lookup(fd_id);
  if (!file) {
    terminate(f, -1);
  } else {
    f->eax = file_length(file);
  }
}

/* Handle read syscall. */
void handle_read(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int) + sizeof(char*) + sizeof(unsigned));
  int fd_id = args[1];
  char* buffer = (char*)args[2];
  unsigned size = args[3];
  validate(buffer, size);
  if (fd_id == 1) {
    terminate(f, -1);
  }

  if (fd_id == 0) {
    f->eax = input_getc();
  } else {
    struct file* file = fd_file_lookup(fd_id);
    if (!file) {
      terminate(f, -1);
    } else {
      f->eax = file_read(file, buffer, size);
    }
  }
}

/* Handle write syscall. */
void handle_write(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int) + sizeof(char*) + sizeof(unsigned));
  int fd_id = args[1];
  char* buffer = (char*)args[2];
  unsigned size = args[3];
  validate(buffer, size);
  if (fd_id == 0) {
    terminate(f, -1);
  }

  if (fd_id == 1) {
    putbuf(buffer, size);
    f->eax = size;
  } else {
    struct file* file = fd_file_lookup(fd_id);
    if (!file) {
      terminate(f, -1);
    } else {
      f->eax = file_write(file, buffer, size);
    }
  }
}

/* Handle seek syscall. */
void handle_seek(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int) + sizeof(unsigned));
  int fd_id = args[1];
  unsigned position = args[2];
  struct file* file = fd_file_lookup(fd_id);
  if (!file) {
    terminate(f, -1);
  } else {
    file_seek(file, position);
  }
}

/* Handle tell syscall. */
void handle_tell(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int));
  int fd_id = args[1];

  struct file* file = fd_file_lookup(fd_id);
  if (!file) {
    terminate(f, -1);
  } else {
    f->eax = file_tell(file);
  }
}

/* Handle close syscall. */
void handle_close(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(int));
  int fd_id = args[1];

  if (fd_id == 0 || fd_id == 1) {
    terminate(f, -1);
  }

  struct fd* fd = fd_lookup(fd_id);
  if (!fd) {
    terminate(f, -1);
  } else {
    if (fd->is_file) {
      struct file* file = fd->content;
      file_close(file);
    } else {
      struct dir* dir = fd->content;
      dir_close(dir);
    }
    hash_delete(&thread_current()->fd_table, &fd->hash_elem);
    free(fd);
  }
}

/* Handle practice syscall. */
void handle_practice(struct intr_frame* f, uint32_t* args) {
  validate(args + 1, sizeof(uint32_t));
  f->eax = args[1] + 1;
}

/* Handle chdir syscall. */
void handle_chdir(struct intr_frame* f, uint32_t* args) {
  char* dir_path = (char*)args[1];
  char* _dir_path = convert_to_abs_path(dir_path);
  bool valid = is_path_valid(_dir_path, true);
  bool not_long = simplify_path(_dir_path);
  if (!not_long || !valid) {
    free(_dir_path);
    f->eax = 0;
    return;
  }

  void* content;
  bool is_file;
  bool stat = get_content(_dir_path, &is_file, &content);
  if (!stat || is_file) {
    free(_dir_path);
    f->eax = 0;
    return;
  }

  struct thread* t = thread_current();
  free(t->cwd);
  t->cwd = malloc(strlen(_dir_path) + 1);
  memcpy(t->cwd, _dir_path, strlen(_dir_path) + 1);
  free(_dir_path);
  f->eax = 1;
}

/* Handle mkdir syscall. */
void handle_mkdir(struct intr_frame* f, uint32_t* args) {
  char* dir_path = (char*)args[1];
  if (!strcmp(dir_path, "")) {
    f->eax = 0;
    return;
  }
  char* _dir_path = convert_to_abs_path(dir_path);
  bool valid = is_path_valid(_dir_path, false);
  bool not_long = simplify_path(_dir_path);
  if (!not_long || !valid) {
    free(_dir_path);
    f->eax = 0;
    return;
  }

  struct dir* container_dir;
  bool found_dir = get_container_dir(_dir_path, &container_dir);
  char* dir_name = malloc(NAME_MAX + 1);
  get_content_name(_dir_path, &dir_name);
  if (!found_dir) {
    f->eax = 0;
  } else {
    bool made = dir_mkdir(container_dir, dir_name);
    if (made)
      f->eax = 1;
    else
      f->eax = 0;
  }

  free(_dir_path);
  dir_close(container_dir);
}

/* Handle readdir syscall. */
void handle_readdir(struct intr_frame* f, uint32_t* args) {
  int fd = (int)args[1];
  char* name = (char*)args[2];

  struct dir* dir = fd_dir_lookup(fd);
  if (!dir) {
    f->eax = 0;
  } else {
    while (true) {
      bool res = dir_readdir(dir, name);
      if (!res) {
        f->eax = 0;
        return;
      } else if (!strcmp(name, ".") || !strcmp(name, "..")) {
        continue;
      } else {
        f->eax = 1;
        return;
      }
    }
  }
}

/* Handle isdir syscall. */
void handle_isdir(struct intr_frame* f, uint32_t* args) {
  int fd = (int)args[1];
  struct dir* dir = fd_dir_lookup(fd);

  if (dir) {
    f->eax = 1;
  } else {
    f->eax = 0;
  }
}

/* Handle inumber syscall. */
void handle_inumber(struct intr_frame* f, uint32_t* args) {
  int fd_id = (int)args[1];
  struct fd* fd = fd_lookup(fd_id);

  if (fd->is_file) {
    struct file* file = (struct file*)fd->content;
    f->eax = file_get_inode(file)->sector;
  } else {
    struct dir* dir = (struct dir*)fd->content;
    f->eax = dir_get_inode(dir)->sector;
  }
}