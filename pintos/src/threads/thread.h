#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/fixed-point.h"
#include <threads/synch.h>
#include <hash.h>

#define CWD_MAX_LENGTH 200

/* States in a thread's life cycle. */
enum thread_status {
  THREAD_RUNNING, /* Running thread. */
  THREAD_READY,   /* Not running but ready to run. */
  THREAD_BLOCKED, /* Waiting for an event to trigger. */
  THREAD_DYING    /* About to be destroyed. */
};

/* Stores information of a file descriptor (fd). */
struct fd {
  int fd_id;     /* File descriptor ID. User will work with this
                                 not file* or other stuff. */
                 //   struct file* file;          /* This has the main information of a file
                 //                                 descriptor */
  void* content; /* A pointer to the dir/file. */
  bool is_file;  /* File or dir? */
  struct hash_elem hash_elem; /* Hash table element */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Stores information about child process. This is shared between
   the child and the parent. The lock is needed when parent is terminating
   the child is about to exit. We have to ensure that only one of them can
   access the data in the struct at any time. Consider a situation where
   the child has validated the existence of the struct and is about to write
   the exit code when context switch happens and parents terminates (thus
   freeing the struct, resulting in a page fault in the child process. */
struct child {
  struct hash_elem hash_elem; /* Hash element for hash table */
  tid_t child_tid;
  struct thread* child_thread;
  int exit_code;         /* Child will store its exit code here. */
  struct semaphore sema; /* Parent will sema_down on it when it wants
                                  to wait for the child, child will sema_up
                                  it when finished. 
                                  Also when creating a new process using exec
                                  we use this to ensure child's successful
                                  startup before resuming the parent. */
  struct lock lock;      /* Is used to synchronize shared data accessing
                                  between child and parent. */
};

/* Thread priorities. */
#define PRI_MIN 0      /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63     /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread {
  /* Owned by thread.c. */
  tid_t tid;                  /* Thread identifier. */
  enum thread_status status;  /* Thread state. */
  char name[16];              /* Name (for debugging purposes). */
  uint8_t* stack;             /* Saved stack pointer. */
  int priority;               /* Priority. */
  struct list_elem allelem;   /* List element for all threads list. */
  int next_fd_id;             /* ID of next file descriptor will be this. */
  struct hash fd_table;       /* Hash table containing file descriptors */
  struct file* exec_file;     /* Process will store its executable file here
                                 to file_deny_write it. */
  struct child* parent_child; /* Points to the child struct in its parent
                                 which is related to. */
  struct hash children;       /* Hash table containing children info. */
  /* Shared between thread.c and synch.c. */
  char* cwd;             /* Current working directory of this thread. */
  struct list_elem elem; /* List element. */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t* pagedir; /* Page directory. */
#endif

  /* Owned by thread.c. */
  unsigned magic; /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void* aux);
tid_t thread_create(const char* name, int priority, thread_func*, void*);

void thread_block(void);
void thread_unblock(struct thread*);

struct thread* thread_current(void);
tid_t thread_tid(void);
const char* thread_name(void);

void thread_exit(int) NO_RETURN;
void thread_yield(void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread* t, void* aux);
void thread_foreach(thread_action_func*, void*);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

#endif /* threads/thread.h */
