#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/user/syscall.h"
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
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);
static struct mmap_entry *allocate_mmap(struct file *file);
static struct mmap_entry *lookup_mmap(mapid_t mapid);
static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);


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

static bool check_right_uvaddr(void * add){
  return (add != NULL && is_user_vaddr (add));
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
  thread_current()->temp_stack = f->esp;
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
      // printf("syscall_read\n");
      int fd = ((int *)f->esp)[1];

      if(!check_right_uvaddr((void *)(((int*)f->esp)[2]))){
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

      if(!check_right_uvaddr((void *)(((int*)f->esp)[2]))){
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

    case SYS_MMAP:
    {
      int fd = ((int *)f->esp)[1];

      if(!check_right_add(f->esp + 4)){
        syscall_exit(-1);
        break;
      }

      void *addr = (void *)(((int*)f->esp)[2]);
      void *overlapped = lookup_page(addr);
      /* For base error case like NULL, overlapped and so on */
      if (!addr || !(addr == pg_round_down(addr)) || overlapped){
        f->eax = -1;
        break;
      }
      if (!is_user_vaddr(addr)) {
        syscall_exit(-1);
        break;
      }

      f->eax = (mapid_t)syscall_mmap(fd, addr);
      break;
    }

    case SYS_MUNMAP:
    {
      mapid_t mapid = (mapid_t)((int *)f->esp)[1];
      syscall_munmap(mapid);
      break;
    }

    case SYS_CHDIR:
    {
      const char *dir = (char *)(((int *)f->esp)[1]);
      f->eax = (bool)syscall_chdir(dir);
      break;
    }

    case SYS_MKDIR:
    {
      const char *dir = (char *)(((int *)f->esp)[1]);
      f->eax = (bool)syscall_mkdir(dir);
      break;
    }

    case SYS_READDIR:
    {
      int fd = ((int *)f->esp)[1];
      char *name = (char *)(((int *)f->esp)[2]);
      f->eax = (bool)syscall_readdir(fd, name);
      break;
    }

    case SYS_ISDIR:
    {
      int fd = ((int *)f->esp)[1];
      f->eax = (bool)syscall_isdir(fd);
      break;
    }

    case SYS_INUMBER:
    {
      int fd = ((int *)f->esp)[1];
      f->eax = (int)syscall_inumber(fd);
      break;
    }

    default:
    {
      syscall_exit(-1);
      break;
    }
  }
}


bool syscall_chdir(const char *dir){
  return 1;  
}

bool syscall_mkdir(const char *dir) {
  return 1;  
}

bool syscall_readdir(int fd, char name[READDIR_MAX_LEN + 1]) {
  return 1;  
}

bool syscall_isdir(int fd) {
  return 1;  
}

int syscall_inumber(int fd) {
  return 1;  
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

  if(!find){
    status = -1;
  }
  else
    sema_up(&member->sema);

  /* return status; */
  printf("%s: exit(%d)\n", file_name, status);

  thread_exit();
}

int syscall_open(const char *file){
  struct thread *t = thread_current();

  lock_acquire(&t->file_list_lock);
  
  struct list_elem *e;
  int i = 0;
  struct file* file_p = filesys_open(file);

  if(file_p == NULL){
    lock_release(&t->file_list_lock);
    return -1;
  }

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

mapid_t syscall_mmap(int fd, void *addr){
  struct thread *t = thread_current();
  struct list_elem *e;
  struct fd *_fd = NULL;
  struct fd *found = NULL;

  if (fd == 1 || fd == 0)
    return -1;

  /* Find the right file */
  for(e = list_begin(&t->file_list); e != list_end(&t->file_list); e = list_next(e)){
    _fd = list_entry(e, struct fd, elem);
    if(_fd->fd == fd){
      found = _fd;
      break;
    }
  }
  if (!found)
    return -1;

  /* And check the re_open handling */
  lock_acquire(&filesys_lock);
  struct file *re_file = file_reopen(found->file_p);
  lock_release(&filesys_lock);

  /* Allocate map_id and insert the mmap_table */
  struct mmap_entry *me = allocate_mmap(re_file);

  int filesize = file_length(re_file);
  int offset1 = 0;
  int offset2 = 0;

  /* Check the right space which can be allocated */
  while(filesize > offset1){
    if(pagedir_get_page(t->pagedir, addr + offset1))
      return -1;
    offset1 += PGSIZE;
  }

  if(re_file == NULL)
    return -1;
  else{
    while(filesize > 0){
      size_t page_zero_bytes = filesize < PGSIZE ? PGSIZE-filesize : 0;    
      locate_mmap_page(addr, re_file, offset2, page_zero_bytes);
      offset2 += PGSIZE;
      filesize -= PGSIZE;
      addr += PGSIZE;
    }
  }
  return me->mapid;
}

void syscall_munmap(mapid_t mapid) {
  struct mmap_entry *me = NULL;
  struct file *file = NULL;

  /* Find the right mapped_file */
  me = lookup_mmap(mapid);
  if(!me)
    return;

  file = me->file;
  list_remove(&me->elem);
  free(me);

  /* For file_unmap handling */
  file_unmap(file);
}

void file_unmap(struct file *file) {
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct list_elem *e;
  struct page_entry *pe;
  size_t page_read_bytes;
  bool is_dirty;
  lock_acquire(&filesys_lock);
  file_seek(file, 0);

  for (e = list_begin(page_table); e != list_end(page_table); e = list_next(e)) {
    pe = list_entry(e, struct page_entry, elem);
    if (pe->file == file) {
      is_dirty = pagedir_is_dirty(t->pagedir, pe->vaddr);
      if (is_dirty) {
        /* If we changed the file(check the dirty bit), we write them in that file */
        page_read_bytes = PGSIZE - pe->page_zero_bytes;
        file_write(file, pe->vaddr, page_read_bytes);
      }
      if (pe->location == FILE)
        /* Only free the page in sup_page_table, don't need to free the frame or page_dir */
        table_free_page(pe->vaddr);
    }
  }
  lock_release(&filesys_lock);
}

static struct mmap_entry *allocate_mmap(struct file *file) {
  /* Allocate the map_id and insert me in the mmap_table */
  struct list_elem *e;
  struct thread *t = thread_current();
  struct list *mmap_table = &t->mmap_table;
  struct mmap_entry *me = NULL;
  mapid_t mapid = 0;
  list_sort(mmap_table, sort, NULL);
  for (e = list_begin(mmap_table); e != list_end(mmap_table); e = list_next(e)) {
    me = list_entry(e, struct mmap_entry, elem);
    if (me->mapid == mapid) {
      mapid++;
      continue;
    }
    break;
  }
  me = (struct mmap_entry *)calloc(1, sizeof(struct mmap_entry));
  me->mapid = mapid;
  me->file = file;
  list_push_back(mmap_table, &me->elem);
  return me;
}

static struct mmap_entry *lookup_mmap(mapid_t mapid) {
  struct mmap_entry *me = NULL;
  struct mmap_entry *found = NULL;
  struct thread *t = thread_current();
  struct list *mmap_table = &t->mmap_table;
  struct list_elem *e;

  for(e = list_begin(mmap_table); e != list_end(mmap_table); e = list_next(e)){
    me = list_entry(e, struct mmap_entry, elem);
    if(me->mapid == mapid){
      found = me;
      break;
    }
  }
  return found;
}

static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct mmap_entry *me_a = list_entry(a, struct mmap_entry, elem);
  struct mmap_entry *me_b = list_entry(b, struct mmap_entry, elem);
  return me_a->mapid < me_b->mapid;
}

bool valid_file_ptr(const char *file) {
  if (!file)
    return 0;
  if((int)file > 0x8084000 && (int)file < 0x40000000)
    return 0;
  return 1;
}

