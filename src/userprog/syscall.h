#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "lib/user/syscall.h"
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

// For proj #3
struct mmap_entry {
	mapid_t mapid;
	struct file *file;
	struct list_elem elem;
};

struct list family;

struct lock family_lock;
struct lock filesys_lock;

int syscall_exit(int status);
int syscall_open(const char *file);
int syscall_read(int fd, void *buffer, unsigned size);
int syscall_write(int fd, void *buffer, unsigned size);
void syscall_close(int fd);
mapid_t syscall_mmap(int fd, void *addr);
void syscall_munmap(mapid_t mapid);
void file_unmap(struct file *file);
bool valid_file_ptr(const char *file);

#endif /* userprog/syscall.h */
