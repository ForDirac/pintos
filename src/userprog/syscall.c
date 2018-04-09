#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "lib/string.h"
#include "filesys/file.h"

static void syscall_handler (struct intr_frame *);

/* For Proj.#2 */
struct fd
{
  int fd;
  const char *file_name;
  struct file* file_p;
  struct list_elem elem;
};

struct list file_list;


/* For Proj.#2 */
static bool compare(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct fd *fd_a = list_entry(a, struct fd, elem);
  struct fd *fd_b = list_entry(b, struct fd, elem);
  return fd_a->fd < fd_b->fd;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* For proj.#2 */
  list_init(&file_list);
  struct fd std_in;
  struct fd std_out;
  std_in.fd = 0;
  std_out.fd = 1;
  list_push_back(&file_list, &std_in.elem);
  list_push_back(&file_list, &std_out.elem);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");

  /* enum syscall_nr = *(unsigned int *)f->esp; */

  switch(*(unsigned int *)f->esp) {
    case SYS_HALT:
    {
      shutdown_power_off();
      break;
    }

    case SYS_EXIT:
    {
      process_exit();
      break;
    }

    case SYS_EXEC:
    {
      const char *file = (char *)(((int*)f->esp)[1]);
      tid_t tid = process_execute(file);
      tid_t *temp = (tid_t *)((char *)f->esp + 4);
      *temp = tid;
      f->esp = temp;
      break;
    }

    case SYS_WAIT:
    {
      tid_t tid = ((int *)f->esp)[1];
      process_wait(tid);
      break;
    }

    case SYS_CREATE:
    {
      const char *file = (char *)(((int*)f->esp)[1]);
      off_t initial_size = (off_t)((unsigned int *)f->esp)[2];
      bool success = filesys_create(file, initial_size);
      int *temp = (int *)((char *)f->esp + 8);
      *temp = 0;
      *temp = success;
      f->esp = temp;
      /* f->esp = (char *)f->esp + 8; */
      /* *f->esp = 0; */
      /* *f->esp = success; */
      break;
    }

    case SYS_REMOVE:
    {
      break;
    }

    case SYS_OPEN:
    {
      const char *file = (char *)(((int*)f->esp)[1]);
      struct file* file_p = filesys_open(file);
      struct list_elem *e;
      int i = 0;

      for(e = list_begin(&file_list); e != list_end(&file_list); e = list_next(e)){
        struct fd *_fd = list_entry(e, struct fd, elem);
        if(strcmp(_fd->file_name, file) == 0){
          int *temp = (int *)((char *)f->esp + 4);
          *temp = _fd->fd;
          f->esp = temp;
          return;
        }
      }

      struct fd new_fd;

      for(e = list_begin(&file_list); e != list_end(&file_list); e = list_next(e)){
        struct fd *__fd = list_entry(e, struct fd, elem);
        if (__fd->fd != i){
          new_fd.fd = i;
          new_fd.file_name = file;
          new_fd.file_p = file_p;
          list_insert_ordered(&file_list, &new_fd.elem, compare, NULL);
          break;
        }
        i++;
      }

      if ((size_t)i == list_size(&file_list)){
        new_fd.fd = i;
        new_fd.file_name = file;
        new_fd.file_p = file_p;
        list_push_back(&file_list, &new_fd.elem);
      }

      int *temp = (int *)((char *)f->esp + 4);
      *temp = new_fd.fd;
      f->esp = temp;

      break;
    }

    case SYS_FILESIZE:
    {
      break;
    }

    case SYS_READ:
    {
      int fd = ((int *)f->esp)[1];
      void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      int count = syscall_read(fd, buffer, size);
      int *temp = (int *)((char *)f->esp + 12);
      *temp = count;
      f->esp = temp;
      break;
    }

    case SYS_WRITE:
    {
      int fd = ((int *)f->esp)[1];
      void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      int count = syscall_write(fd, buffer, size);
      int *temp = (int *)((char *)f->esp + 12);
      *temp = count;
      f->esp = temp;
      break;
    }

    case SYS_SEEK:
    {
      break;
    }

    case SYS_TELL:
    {
      break;
    }

    case SYS_CLOSE:
    {
      break;
    }

    default:
    {
      ASSERT(0);
    }
  }
  thread_exit ();
}

int syscall_open(const char *file) {
  filesys_open(file);

}

int syscall_read(int fd, void *buffer, unsigned size) {
  /* struct lock read_lock; */
  /* lock_init(&read_lock); */
  /* lock_acquire(&read_lock); */
  struct list_elem *e;
  struct fd *_fd;

  for(e = list_begin(&file_list); e != list_end(&file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      break;
    }
  }
  return (int)file_read(_fd->file_p, buffer, (off_t)size);
}

int syscall_write(int fd, void *buffer, unsigned size) {
  struct list_elem *e;
  struct fd *_fd;

  for(e = list_begin(&file_list); e != list_end(&file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      break;
    }
  }
  return (int)file_write(_fd->file_p, buffer, (off_t)size);
}

void syscall_close(int fd) {

}
