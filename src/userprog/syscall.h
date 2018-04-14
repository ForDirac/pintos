#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);

/* For proj #2 */
struct member
{
  struct thread *parent;
  tid_t child_tid;
  int exit_status;
  bool is_exit;
  bool success;
  struct list_elem elem;
  struct semaphore sema;
  struct semaphore loading_sema;
};

struct list family;

struct lock family_lock;

int syscall_exit(int status);
int syscall_open(const char *file);
int syscall_read(int fd, void *buffer, unsigned size);
int syscall_write(int fd, void *buffer, unsigned size);
void syscall_close(int fd);
bool valid_file_ptr(char *file);

#endif /* userprog/syscall.h */
