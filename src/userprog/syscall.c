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
#include "lib/kernel/console.h"
#include "devices/input.h"
#include "threads/synch.h"

static void syscall_handler (struct intr_frame *);

/* For Proj.#2 */
/* struct fd */
/* { */
/*   int fd; */
/*   const char *file_name; */
/*   struct file* file_p; */
/*   struct list_elem elem; */
/* }; */


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
  list_init(&family); // To store the parent, child_tid, exit_status, sema
  lock_init(&family_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* printf ("system call!\n"); */

  /* enum syscall_nr = *(unsigned int *)f->esp; */

  switch(*(unsigned int *)f->esp) {

    case SYS_HALT:
    {
      shutdown_power_off();
      break;
    }

    case SYS_EXIT:
    {
      int status = ((int *)f->esp)[1];
      struct thread *t = thread_current();
      struct list_elem *e;
      char *next_p;
      char space[2] = " ";
      bool find = 0;
      struct member *member;

      char *file_name = strtok_r(t->name, space, &next_p);

      lock_acquire(&family_lock);
      for(e = list_begin(&family); e != list_end(&family); e = list_next(e)){
        member = list_entry(e, struct member, elem);
        if(t->tid == member->child_tid){
          member->is_exit = 1;
          member->exit_status = status;
          find = 1;
        }
      }
      lock_release(&family_lock);


      if(!find)
        status = -1;
      else
        sema_up(&member->sema);

      f->eax = status;
      printf("%s: exit(%d)\n", file_name, status);

      thread_exit();
      break;
    }

    case SYS_EXEC:
    {
      /* We suppose that pintos have single thread system! */
      const char *file = (char *)(((int*)f->esp)[1]);

      tid_t tid = process_execute(file);

      f->eax = tid;
      break;
    }

    case SYS_WAIT:
    {
      tid_t tid = ((int *)f->esp)[1];

      int exit_status = process_wait(tid);

      f->eax = exit_status;
      break;
    }

    case SYS_CREATE:
    {
      const char *file = (char *)(((int*)f->esp)[1]);
      off_t initial_size = (off_t)((unsigned int *)f->esp)[2];
      bool success = filesys_create(file, initial_size);
      f->eax = 0;
      f->eax = success;
      break;
    }

    case SYS_REMOVE:
    {
      break;
    }

    case SYS_OPEN:
    {
      const char *file = (char *)(((int*)f->esp)[1]);
      struct thread *t = thread_current();
      struct file* file_p = filesys_open(file);
      struct list_elem *e;
      int i = 0;

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        struct fd *_fd = list_entry(e, struct fd, elem);
        printf("%s\n", file);
        printf("%d\n", _fd->fd);
        if(strcmp(_fd->file_name, file) == 0){
          f->eax = _fd->fd;
          return;
        }
      }

      struct fd new_fd;

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        struct fd *__fd = list_entry(e, struct fd, elem);
        if (__fd->fd != i){
          new_fd.fd = i;
          strlcpy(new_fd.file_name, file, strlen(file)+1);
          new_fd.file_p = file_p;
          list_insert_ordered(&t->file_list, &new_fd.elem, compare, NULL);
          break;
        }
        i++;
      }

      if ((size_t)i == list_size(&t->file_list)){
        new_fd.fd = i;
        strlcpy(new_fd.file_name, file, strlen(file)+1);
        new_fd.file_p = file_p;
        list_push_back(&t->file_list, &new_fd.elem);
      }

      f->eax = new_fd.fd;

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
      f->eax = count;
      break;
    }

    case SYS_WRITE:
    {
      int fd = ((int *)f->esp)[1];
      void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      int count = syscall_write(fd, buffer, size);
      f->eax = count;
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
  /* thread_exit (); */
}

int syscall_open(const char *file) {
  filesys_open(file);

}

int syscall_read(int fd, void *buffer, unsigned size) {
  /* struct lock read_lock; */
  /* lock_init(&read_lock); */
  /* lock_acquire(&read_lock); */
  struct list_elem *e;
  struct fd *_fd = NULL;
  struct thread *t = thread_current();

  ASSERT(fd != 1);

  if (fd == 0){
    uint8_t key;
    uint8_t *temp = buffer;
    unsigned i = 0;
    while((key = input_getc()) != 13){
      if (i >= size){
        break;
      }
      *temp = key;
      temp++;
      i++;
    }
    return i;
  }

  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      break;
    }
  }

  return (int)file_read(_fd->file_p, buffer, (off_t)size);
}

int syscall_write(int fd, void *buffer, unsigned size) {
  struct list_elem *e;
  struct fd *_fd = NULL;
  struct thread *t = thread_current();

  ASSERT(fd != 0);

  if (fd == 1){
    putbuf(buffer, size);
    return size;
  }

  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      break;
    }
  }

  return (int)file_write(_fd->file_p, buffer, (off_t)size);
}

void syscall_close(int fd) {

}
