#include <stdio.h>
#include <list.h>
#include <string.h>
#include <bitmap.h>
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

static bool install_page(void *upage, void *kpage, bool writable);

void page_init(struct list *list) {
	list_init(list);
}

/* If this vaddr's page is in the DISK, pick it up in our page_table */
bool reclamation(void *vaddr, bool user, bool writable){
  void *upage = pg_round_down(vaddr);
  void *kpage;
  bool success;
  struct page_entry *pe = locate_page(upage, PHYS);
  if (user)
    kpage = (void *)palloc_get_page(PAL_USER | PAL_ZERO);
  else
    kpage = (void *)palloc_get_page(PAL_ZERO);
  if (!kpage) {
    // swap and get kpage
    kpage = swap_out(user?PAL_USER|PAL_ZERO:PAL_ZERO);
  }
  swap_in(kpage, pe);
  if (!(success = install_page(upage, kpage, writable))) {
    table_free_page(upage);
    table_free_frame(kpage);
  }
  return success;
}

/* locate the Nomally page_entry in our sup_page_table*/
struct page_entry *locate_page(void *vaddr, int location) {
	struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct page_entry *pe = lookup_page(vaddr);
  if (pe) {
    pe->location = location;
    pe->lazy_loading = 0;
    pe->is_mmap = 0;
    return pe;
  }
  pe = (struct page_entry *)calloc(1, sizeof(struct page_entry));
	pe->vaddr = pg_round_down(vaddr);
	pe->dirty = !!((unsigned)vaddr & PTE_D); // change to boolen_type
	pe->access = !!((unsigned)vaddr & PTE_A); // change to boolen_type
  pe->location = location;
  pe->lazy_loading = 0;
  pe->is_mmap = 0;
	list_push_back(page_table, &pe->elem);
  return pe;
}

/* locate the page_entry which entered in lazy_loading(process.c) section in our sup_page_table*/
struct page_entry *locate_lazy_page(void *vaddr, struct file *file, off_t offset, size_t page_zero_bytes, bool writable) {
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct page_entry *pe = lookup_page(vaddr);
  if (pe) {
    pe->location = FILE;
    pe->lazy_loading = 1;
    pe->is_mmap = 0;
    pe->file = file;
    pe->offset = offset;
    pe->page_zero_bytes = page_zero_bytes;
    pe->writable = writable;
    return pe;
  }
  pe = (struct page_entry *)calloc(1, sizeof(struct page_entry));
  pe->vaddr = pg_round_down(vaddr);
  pe->dirty = !!((unsigned)vaddr & PTE_D); // change to boolen_type
  pe->access = !!((unsigned)vaddr & PTE_A); // change to boolen_type
  pe->location = FILE;
  pe->lazy_loading = 1;
  pe->is_mmap = 0;
  pe->file = file;
  pe->offset = offset;
  pe->page_zero_bytes = page_zero_bytes;
  pe->writable = writable;
  list_push_back(page_table, &pe->elem);
  return pe;
}

/* locate the page_entry which entered in mmap section(syscall.c) in our sup_page_table*/
struct page_entry *locate_mmap_page(void *vaddr, struct file *file, off_t offset, size_t page_zero_bytes) {
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct page_entry *pe = lookup_page(vaddr);
  if (pe) {
    pe->location = FILE;
    pe->is_mmap = 1;
    pe->lazy_loading = 0;
    pe->file = file;
    pe->offset = offset;
    pe->page_zero_bytes = page_zero_bytes;
    pe->writable = 1;
    return pe;
  }
  pe = (struct page_entry *)calloc(1, sizeof(struct page_entry));
  pe->vaddr = pg_round_down(vaddr);
  pe->dirty = !!((unsigned)vaddr & PTE_D);
  pe->access = !!((unsigned)vaddr & PTE_A);
  pe->location = FILE;
  pe->is_mmap = 1;
  pe->lazy_loading = 0;
  pe->file = file;
  pe->offset = offset;
  pe->page_zero_bytes = page_zero_bytes;
  pe->writable = 1;
  list_push_back(page_table, &pe->elem);
  return pe;
}


/* Update our frame_table and base_page_table after making the new_page_entry*/
bool lazy_load_segment(void *vaddr, bool user, bool writable, struct file *file, off_t offset, size_t page_zero_bytes){
  void *upage = pg_round_down(vaddr);
  void *kpage;
  size_t page_read_bytes = PGSIZE - page_zero_bytes;
  struct page_entry *pe = locate_page(upage, PHYS);

  file_seek(file, offset);

  /* Get a page of memory. */
  if (user)
    kpage = (void *)palloc_get_page (PAL_USER | PAL_ZERO);
  else
    kpage = (void *)palloc_get_page(PAL_ZERO);

  if (kpage == NULL){
    kpage = swap_out(user?PAL_USER|PAL_ZERO:PAL_ZERO);
  }

  /* Load this page. */
  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
      table_free_page(upage);
      table_free_frame(kpage);
      return 0;
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  insert_frame_table(kpage, pe);

  /* Add the page to the process's address space. If this action is failed, free all */
  if (!install_page(upage, kpage, writable))
    {
      table_free_page(upage);
      table_free_frame(kpage);
      return 0; 
    }
  return 1;
}

struct page_entry *lookup_page(uint32_t *vaddr) {
  void *upage = pg_round_down(vaddr);
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct list_elem *e;
  struct page_entry *pe = NULL;
  struct page_entry *found = NULL;
  for (e = list_begin(page_table); e != list_end(page_table); e = list_next(e)) {
    pe = list_entry(e, struct page_entry, elem);
    if (upage == pg_round_down(pe->vaddr)) {
      found = pe;
      break;
    }
  }
  return found;
}

bool stack_growth(void *vaddr, bool user, bool writable){
  void *upage = pg_round_down(vaddr);
  struct thread *cur = thread_current();
  // allocate a page from a USER_POOL, and add an entry to frame_table
  void* frame = palloc_get_page(user?PAL_USER|PAL_ZERO:PAL_ZERO);
  if(frame == NULL){
    return 0;
  }
  else{
    struct page_entry *pe = locate_page(upage, PHYS);
    insert_frame_table(frame, pe);
    //add the page to the process's address space
    if(!pagedir_set_page(cur->pagedir, upage, frame, writable)){
      //free the frame - set failure
      table_free_page(upage);
      table_free_frame(frame);
      return 0;
    }
  }
  return 1;
}

void table_free_page(void *vaddr) {
  if (!vaddr)
    return;
  void *upage = pg_round_down(vaddr);
  struct page_entry *pe = lookup_page(upage);
  list_remove(&pe->elem);
  free(pe);
}

static bool install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
