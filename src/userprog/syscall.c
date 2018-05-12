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
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);


/* For Proj.#2 */
static bool compare(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct fd *fd_a = list_entry(a, struct fd, elem);
  struct fd *fd_b = list_entry(b, struct fd, elem);
  return fd_a->fd < fd_b->fd;
}

static bool check_right_add(void * add){
  bool right = 1;
  int i;
  struct thread *t = thread_current();
  if(!(add))
    return 0;
  if( add > PHYS_BASE - 12)
    return 0;
  if(add < (void *)0x8048000)
    return 0;

  for (i = 0; i < 4; i++){
    void *p = (void *)pagedir_get_page(t->pagedir, add+i);

    if( add+i > PHYS_BASE - 12)
      right = 0;
    if(!p)
      right = 0;
    if(add+i < (void *)0x8048000)
      right = 0;
    if(!(add+i))
      right = 0;
  }
  return right;
}

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* For proj.#2 */
  list_init(&family); // To store the parent, child_tid, exit_status, sema
  lock_init(&family_lock);
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  if(!check_right_add(f->esp)){
    syscall_exit(-1);
    return;
  }

  switch(*(unsigned int *)f->esp) {

    case SYS_HALT:
    {
      shutdown_power_off();
      break;
    }

    case SYS_EXIT:
    {
      int status = ((int *)f->esp)[1];

      syscall_exit(status);
      break;
    }

    case SYS_EXEC:
    {
      /* We suppose that pintos have single thread system! */
      if(!check_right_add(f->esp + 4)){
        syscall_exit(-1);
        break;
      }

      const char *file = (char *)(((int*)f->esp)[1]);
      if (!valid_file_ptr(file)) {
        syscall_exit(-1);
        break;
      }

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
      if(!check_right_add(f->esp + 4)){
        syscall_exit(-1);
        break;
      }

      const char *file = (char *)(((int*)f->esp)[1]);
      if (!valid_file_ptr(file)){
        syscall_exit(-1);
        break;
      }
      off_t initial_size = (off_t)((unsigned int *)f->esp)[2];
      lock_acquire(&filesys_lock);
      bool success = filesys_create(file, initial_size);
      lock_release(&filesys_lock);
      f->eax = 0;
      f->eax = success;
      break;
    }

    case SYS_REMOVE:
    {
      if(!check_right_add(f->esp + 4)){
        syscall_exit(-1);
        break;
      }

      const char *file = (char *)(((int*)f->esp)[1]);
      if (!valid_file_ptr(file)) {
        syscall_exit(-1);
        break;
      }

      lock_acquire(&filesys_lock);
      bool success = filesys_remove(file);
      lock_release(&filesys_lock);
      f->eax = 0;
      f->eax = success;
      break;
    }

    case SYS_OPEN:
    {
      if(!check_right_add(f->esp + 4)){
        syscall_exit(-1);
        break;
      }

      const char *file = (char *)(((int*)f->esp)[1]);
      if (!valid_file_ptr(file)) {
        syscall_exit(-1);
        break;
      }

      lock_acquire(&filesys_lock);
      int fd = syscall_open(file);
      lock_release(&filesys_lock);
      f->eax = fd;
      break;
    }

    case SYS_FILESIZE:
    {
      int fd = ((int *)f->esp)[1];
      struct list_elem *e;
      struct thread *t = thread_current();
      struct fd *_fd = NULL;

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        _fd = list_entry(e, struct fd, elem);
        if(_fd->fd == fd){
          break;
        }
      }

      lock_acquire(&filesys_lock);
      f->eax = file_length(_fd->file_p);
      lock_release(&filesys_lock);
      break;
    }

    case SYS_READ:
    {
      int fd = ((int *)f->esp)[1];

      if(!check_right_add((void *)(((int*)f->esp)[2]))){
        syscall_exit(-1);
        break;
      }

      void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      int count = syscall_read(fd, buffer, size);
      if (count == -2) {
        syscall_exit(-1);
        break;
      }

      f->eax = count;
      break;
    }

    case SYS_WRITE:
    {
      int fd = ((int *)f->esp)[1];

      if(!check_right_add((void *)(((int*)f->esp)[2]))){
        syscall_exit(-1);
        break;
      }

      void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      int count = syscall_write(fd, buffer, size);
      f->eax = count;
      break;
    }

    case SYS_SEEK:
    {
      int fd = ((int *)f->esp)[1];
      off_t position = ((off_t *)f->esp)[2];
      struct list_elem *e;
      struct thread *t = thread_current();

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        struct fd *_fd = list_entry(e, struct fd, elem);
        if(_fd->fd == fd){
          lock_acquire(&filesys_lock);
          file_seek(_fd->file_p, position);
          lock_release(&filesys_lock);
          break;
        }
      }
      break;
    }

    case SYS_TELL:
    {
      int fd = ((int *)f->esp)[1];
      struct list_elem *e;
      struct thread *t = thread_current();
      off_t cur_p = -1;

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        struct fd *_fd = list_entry(e, struct fd, elem);
        if(_fd->fd == fd){
          lock_acquire(&filesys_lock);
          cur_p = file_tell(_fd->file_p);
          lock_release(&filesys_lock);
          break;
        }
      }

      f->eax = cur_p;

      break;
    }

    case SYS_CLOSE:
    {
      int fd = ((int *)f->esp)[1];
      struct list_elem *e;
      struct thread *t = thread_current();
      bool valid_fd = 0;

      for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
        struct fd *_fd = list_entry(e, struct fd, elem);
        if(_fd->fd == fd){
          valid_fd = 1;
          lock_acquire(&filesys_lock);
          file_close(_fd->file_p);
          lock_release(&filesys_lock);

          list_remove(e);
          free(_fd);
          break;
        }
      }
      if (!valid_fd) {
        syscall_exit(-1);
        break;
      }
      break;
    }

    default:
    {
      syscall_exit(-1);
      break;
    }
  }
}

int syscall_exit(int status){
  struct thread *t = thread_current();
  struct list_elem *e;
  bool find = 0;
  struct member *member = NULL;
  char *next_p;
  char space[2] = " ";

  char *file_name = strtok_r(t->name, space, &next_p);

  if(status < 0 || status > 255){
    status = -1;
  }

  lock_acquire(&family_lock);
  for(e = list_begin(&family); e != list_end(&family); e = list_next(e)){
    member = list_entry(e, struct member, elem);
    if(t->tid == member->child_tid){
      member->is_exit = 1;
      member->exit_status = status;
      find = 1;
      break;
    }
  }
  lock_release(&family_lock);

  if(!find)
    status = -1;
  else
    sema_up(&member->sema);

  /* return status; */
  printf("%s: exit(%d)\n", file_name, status);

  thread_exit();
}

int syscall_open(const char *file){
  struct thread *t = thread_current();
  struct file* file_p = filesys_open(file);
  struct list_elem *e;
  int i = 0;

  if(file_p == NULL){
    return -1;
  }

  lock_acquire(&t->file_list_lock);

  if(list_empty(&t->file_list)){
    /* Insert the Default values which are STD_IN & STD_OUT and initialize them */
    struct fd *std_in = (struct fd *) malloc(sizeof(struct fd));
    struct fd *std_out = (struct fd *) malloc(sizeof(struct fd));

    std_in->fd = 0;
    std_in->file_name = "STD_IN";
    std_in->file_p = NULL;

    std_out->fd = 1;
    std_out->file_name = "STD_OUT";
    std_out->file_p = NULL;

    list_push_back(&t->file_list, &std_in->elem);
    list_push_back(&t->file_list, &std_out->elem);
  }

  struct fd *new_fd = (struct fd *)malloc(sizeof(struct fd));

  //Insert the file_list in this opened file info
  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    struct fd *__fd = list_entry(e, struct fd, elem);
    if (__fd->fd != i){
      new_fd->fd = i;
      new_fd->file_name = file;
      new_fd->file_p = file_p;
      list_insert_ordered(&t->file_list, &new_fd->elem, compare, NULL);
      break;
    }
    i++;
  }

  if ((size_t)i == list_size(&t->file_list)){
    new_fd->fd = i;
    new_fd->file_name = file;
    new_fd->file_p = file_p;
    list_push_back(&t->file_list, &new_fd->elem);
  }
  lock_release(&t->file_list_lock);

  return new_fd->fd;
}


int syscall_read(int fd, void *buffer, unsigned size) {
  struct list_elem *e;
  struct fd *_fd = NULL;
  struct thread *t = thread_current();
  bool find = 0;

  if (fd < 0) {
    return -2;
  }

  if (fd == 1){
    return -1;
  }


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
      find = 1;
      break;
    }
  }

  if(!find){
    return -2;
  }

  lock_acquire(&filesys_lock);
  int bytes_read = (int)file_read(_fd->file_p, buffer, (off_t)size);
  lock_release(&filesys_lock);

  return bytes_read;
}

int syscall_write(int fd, void *buffer, unsigned size) {
  struct list_elem *e;
  struct fd *_fd = NULL;
  struct thread *t = thread_current();
  bool find = 0;

  if(fd == 0){
    return -1;
  }

  if(fd == 1){
    putbuf(buffer, size);
    return size;
  }

  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      find = 1;
      break;
    }
  }

  if(!find){
    return -1;
  }

  lock_acquire(&filesys_lock);
  int bytes_write = (int)file_write(_fd->file_p, buffer, (off_t)size);
  lock_release(&filesys_lock);
  return bytes_write;
}

bool valid_file_ptr(const char *file) {
  if (!file)
    return 0;
  if((int)file > 0x8084000 && (int)file < 0x40000000)
    return 0;
  return 1;
}
