#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define MAX_ARGS 128
#define WORD_SIZE sizeof(void*)

struct arguments {
  int argc;
  char* argv[MAX_ARGS];
};

tid_t process_execute(const char* file_name);
int process_wait(tid_t);
void process_exit(int);
void process_activate(void);
struct child* child_lookup(tid_t);
struct fd* fd_lookup(int);
struct file* fd_file_lookup(int);
struct dir* fd_dir_lookup(int);

#endif /* userprog/process.h */
