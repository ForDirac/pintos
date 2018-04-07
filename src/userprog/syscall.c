#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");

  enum syscall_nr = *(unsigned int *)f->esp;

  switch(syscall_nr) {
    case SYS_HALT:
      shutdown_power_off();
      break;

    case SYS_EXIT:
      process_exit();
      break;

    case SYS_EXEC:
      const char *file_name = (char *)(((int*)f->esp)[1]);
      tid_t tid = process_execute(file_name);
      f->esp = (char *)f->esp + 4;
      *f->esp = tid;
      break;

    case SYS_WAIT:
      tid_t tid = ((int *)f->esp)[1];
      process_wait(tid);
      break;

    case SYS_OPEN:
      const char *file = (char *)(((int*)f->esp)[1]);
      syscall_open(file);
      break;

    case SYS_WRITE:
      int fd = ((int *)f->esp)[1];
      const void *buffer = (void *)(((int*)f->esp)[2]);
      unsigned size = ((unsigned int *)f->esp)[3];
      syscall_write(fd, buffer, size);
      break;

  }
  thread_exit ();
}

void syscall_open(const char *file){
  
}


void syscall_write(int fd, const void *buffer, unsigned size){


  strlcpy()
}
