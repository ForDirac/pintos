#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* For proj #2 */
struct list *tid_list;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;

  /* For proj.#2 */
  // struct thread *t = thread_current();

  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL){
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);
  // printf("process_execute(): fn_copy(%s)\n", fn_copy);
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  // printf("process_execute(): tid(%d)\n", tid);

  // Original code, but if this code is exited, it can make error
  // if (tid == TID_ERROR)
  //   palloc_free_page (fn_copy);

 return tid;
}

/* Find the right member in the family list */
struct member *lookup_child(tid_t tid) {
  struct list_elem *e;
  struct member *member;
  bool valid = 0;
  lock_acquire(&family_lock);
  for (e = list_begin(&family); e != list_end(&family); e = list_next(e)) {
    member = list_entry(e, struct member, elem);
    if (tid == member->child_tid) {
      valid = 1;
      break;
    }
  }
  lock_release(&family_lock);
  if (!valid)
    return NULL;
  return member;
}

/* Return the loading_result which success or not */
static void loading_result(bool success) {
  struct list_elem *e;
  struct member *member;
  struct thread *t = thread_current();

  lock_acquire(&family_lock);
  for (e = list_begin(&family); e != list_end(&family); e = list_next(e)) {
    member = list_entry(e, struct member, elem);
    if (t->tid == member->child_tid) {
      // printf("loading_result(): tid(%d) loading_sema value(%d)\n", t->tid, member->loading_sema.value);
      member->success = success;
      sema_up(&member->loading_sema);
      // printf("loading_result(): tid(%d) loading_sema value after sema_up(%d)\n", t->tid, member->loading_sema.value);
      break;
    }
  }
  lock_release(&family_lock);
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* For Proj.#2 */
  int argc = 0;
  char *argv[100];
  char *arg_p;
  size_t str_len = 0;
  char *next_p;
  char *s = file_name;
  char space[2] = " ";
  int i;

  arg_p = strtok_r(s, space, &next_p);
  if (arg_p != NULL) {
    /* To add the NULL text in str_length */
    str_len += strlen(arg_p) + 1;
    argv[argc] = arg_p;
    argc++;
  }

  while(arg_p) {
    arg_p = strtok_r(NULL, space, &next_p);
    if (arg_p != NULL) {
      str_len += strlen(arg_p) + 1;
      argv[argc] = arg_p;
      argc++;
    }
  }
  char *file_rename = argv[0];
  char *exit_file_name = (char *)malloc(strlen(file_rename)+1);
  strlcpy(exit_file_name, file_rename, strlen(file_rename)+1);
  struct thread *t = thread_current();
  strlcpy(t->file_name, file_rename, strlen(file_rename)+1);

  int total = 8 + (argc+2)*4 + (str_len%4 != 0) * (4-(str_len%4)) + str_len;
  int esp[100];
  memset(esp, 0, 400);

  char *current = (char *)esp + total - str_len;
  int virtual_current = (int)PHYS_BASE - str_len;

  esp[1] = argc;
  esp[2] = (int)PHYS_BASE - total + 12;

  for (i=0; i<argc; i++){
    if(argv[i] == NULL)
      break;
    strlcpy(current, argv[i], strlen(argv[i]) + 1);
    esp[i+3] = virtual_current;
    current += strlen(argv[i]) + 1;
    virtual_current += strlen(argv[i]) + 1;
  }
  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  // printf("start_process(): file_rename(%s)\n", file_rename);
  lock_acquire(&filesys_lock);
  success = load (file_rename, &if_.eip, &if_.esp);
  lock_release(&filesys_lock);

  // printf("start_process(): success(%s)\n", success? "true":"false");
  if(thread_current()->tid != 1 || thread_current()->tid != 0){
    loading_result(success);
  }

  /* If load failed, quit. */
  palloc_free_page (file_name);

  /* For Proj.#2 
  If don't success, this thread must be exited*/
  if (!success) {
    // printf("%s\n", "exit by loading failed");
    printf("%s: exit(%d)\n", exit_file_name, -1);
    free(exit_file_name);
    thread_exit();
  }
  else
    free(exit_file_name);

  // printf("start_process(): DONE\n");
  memcpy((char *)(PHYS_BASE-(void *)total), (char *)esp, (size_t)total);
  if_.esp = PHYS_BASE - total;

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  struct list_elem *e;
  bool child_exists = 0;
  struct member *member;

  lock_acquire(&family_lock);
  for(e=list_begin(&family); e != list_end(&family); e = list_next(e)){
    member = list_entry(e, struct member, elem);
    if(child_tid == member->child_tid){
      child_exists = 1;
      break;
    }
  }
  lock_release(&family_lock);

  if (child_exists){
    sema_down(&member->sema);
  }
  else{
    // printf("don't exist!\n");
    return -1;
  }

  //child is alive yet!(error)
  if(!member->is_exit){
    lock_acquire(&family_lock);
    list_remove(&member->elem);
    lock_release(&family_lock);
    free(member);
    // printf("alive yet!\n");
    return -1;
  }

  int exit_status = member->exit_status;
  // printf("in the wait function exit_status : %d\n",exit_status);

  lock_acquire(&family_lock);
  list_remove(&member->elem);
  lock_release(&family_lock);
  free(member);

  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Find the remain member which current thread is same as member's parents in family_list */
  struct list_elem *e; 
  struct member *member;

  lock_acquire(&family_lock); 
  for (e = list_begin(&family); e !=list_end(&family); e = list_next(e)){ 
    member = list_entry(e, struct member, elem); 
    if (cur == member->parent) { 
      list_remove(&member->elem); 
      free(member);
      break;
    } 
  }
  lock_release(&family_lock); 
  
  /* Find the remain fd structure, and execute file_close and free
  if fd->file_p is NULL(it means fd is std_in or std_out), we just execute free */
  struct fd *fd;

  while(!list_empty(&cur->file_list)){
    fd = list_entry(list_pop_front(&cur->file_list), struct fd, elem);
    if(fd->file_p == NULL){
      free(fd);
    }
    else{
      if (inode_is_dir(file_get_inode(fd->file_p))){
        // dir_close((struct dir *)fd->file_p);
      }
      else
        file_close(fd->file_p);
      free(fd);
    }
  }

  /* Find the remain mmap_entry, then excute file_unmap and free them all. */
  struct list *mmap_table = &cur->mmap_table;
  struct mmap_entry *me;
  while(!list_empty(mmap_table)) {
    e = list_pop_front(mmap_table);
    me = list_entry(e, struct mmap_entry, elem);
    file_unmap(me->file);
    free(me);
  }

  /* Find the remain sup_page_entry, then free them all. */
  struct list *page_table = &cur->sup_page_table;
  struct page_entry *pe;
  while(!list_empty(page_table)) {
    e = list_pop_front(page_table);
    pe = list_entry(e, struct page_entry, elem);
    free(pe);
    // TODO : free the entry which is in frame or swap table
  }

  /* If current thread has execute_file, we execute allow_write and file_close for read_only_child cases */
  if(cur->execute_f){
    file_allow_write(cur->execute_f);
    file_close(cur->execute_f);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      file_close(file);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      // printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  /* Set up the file_deny because this file is opened at this time. 
  And this file is regarded as current thread's execute_file */
  file_deny_write(file);
  thread_current()->execute_f = file;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      if (!locate_lazy_page(upage, file, ofs, page_zero_bytes, writable))
        return false;

      // if (page_zero_bytes == PGSIZE){
      //   locate_lazy_page(upage, ALL_ZERO, file);
      // }
      // else if(page_read_bytes == PGSIZE){
      //   locate_lazy_page(upage, EXE_FILE, file);
      // }
      // else{
      //   lazy_load_segment(upage, 1, 0, file, page_zero_bytes, page_read_bytes);


        // /* Get a page of memory. */
        // uint8_t *kpage = palloc_get_page (PAL_USER);
        // if (kpage == NULL)
        //   return false;

        // /* Load this page. */
        // if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        //   {
        //     palloc_free_page (kpage);
        //     return false; 
        //   }
        // memset (kpage + page_read_bytes, 0, page_zero_bytes);

        // /* Add the page to the process's address space. */
        // if (!install_page (upage, kpage, writable)) 
        //   {
        //     palloc_free_page (kpage);
        //     return false; 
        //   }
      // }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      ofs += page_read_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else{
        palloc_free_page (kpage);
      }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
