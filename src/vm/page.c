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

bool new_page(void *vaddr, bool user, bool writable) {
  void *upage = pg_round_down(vaddr); // vaddr's page_num
  void *kpage; // frame
  bool success;
  // int location;
  // Obtain a frame to store the page
  if (user)
    kpage = (void *)palloc_get_page(PAL_USER | PAL_ZERO);
  else
    kpage = (void *)palloc_get_page(PAL_ZERO);
  if (!kpage) {
  	// swap and get kpage
    // location = DISK;
    kpage = swap_out();
  }
  else{
    // location = PHYS;
    struct page_entry *pe = locate_page(vaddr, PHYS);
    insert_frame_table(kpage, pe);
  }
  // Locate the page that faulted in the supplemental page table.
  // locate_page(vaddr, location);
  // Reset page table
  success = install_page(upage, kpage, writable);
  printf("install_page result in new_page: %s\n", success ? "SUCCESS" : "FAILURE");  // for debugging
  return success;
}

bool reclamation(void *vaddr, bool user, bool writable){
  void *upage = pg_round_down(vaddr);
  void *kpage;
  bool success;
  int location;
  struct page_entry *pe;
  if (user)
    kpage = (void *)palloc_get_page(PAL_USER | PAL_ZERO);
  else
    kpage = (void *)palloc_get_page(PAL_ZERO);
  if (!kpage) {
    // swap and get kpage
    // location = DISK;
    kpage = swap_out();
  }
  else{
    location = PHYS;
    pe = locate_page(vaddr, location);
    insert_frame_table(kpage, pe);
  }

  swap_in(kpage);
  success = install_page(upage, kpage, writable);
  printf("install_page result in reclamation: %s\n", success ? "SUCCESS" : "FAILURE");  // for debugging
  return success;
}

static bool install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}

void free_page(void *vaddr) {
  struct page_entry *pe = lookup_page(vaddr);
  void *upage = pg_round_down(vaddr);
  palloc_free_page(upage);
  list_remove(&pe->elem);
  free(pe);
}

struct page_entry *locate_page(void *vaddr, int location) {
	struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
	struct page_entry *pe = (struct page_entry *)malloc(sizeof(struct page_entry));
	pe->vaddr = vaddr;
	pe->dirty = !!((unsigned)vaddr & PTE_D); // change to boolen_type
	pe->access = !!((unsigned)vaddr & PTE_A); // change to boolen_type
  pe->location = location;
	list_push_back(page_table, &pe->elem);
  return pe;
}

struct page_entry *locate_lazy_page(void *vaddr, int lazy_type, struct file *file) {
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct page_entry *pe = (struct page_entry *)malloc(sizeof(struct page_entry));
  pe->vaddr = vaddr;
  pe->lazy_loading = 1;
  pe->lazy_type = lazy_type;
  pe->file = file;
  pe->dirty = !!((unsigned)vaddr & PTE_D); // change to boolen_type
  pe->access = !!((unsigned)vaddr & PTE_A); // change to boolen_type
  pe->location = FILE;
  list_push_back(page_table, &pe->elem);
  return pe;
}

bool lazy_load_segment(void *vaddr, bool user, bool writable, int lazy_type, struct file *file){
  void *upage = pg_round_down(vaddr);
  void *kpage;
  size_t page_read_bytes;
  size_t page_zero_bytes;

  if(lazy_type == ALL_ZERO){
    page_zero_bytes = PGSIZE;
    page_read_bytes = 0;  
  }
  else if(lazy_type == EXE_FILE){
    page_zero_bytes = 0;
    page_read_bytes = PGSIZE; 
  }
  else{
    ASSERT(0);
  }
  /* Get a page of memory. */
  if (user)
    kpage = (void *)palloc_get_page(PAL_USER);
  else
    kpage = (void *)palloc_get_page(0);
  
  if (kpage == NULL)
    return 0;

  /* Load this page. */
  if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
    {
      palloc_free_page (kpage);
      return 0; 
    }
  memset (kpage + page_read_bytes, 0, page_zero_bytes);

  /* Add the page to the process's address space. */
  if (!install_page (upage, kpage, writable)) 
    {
      palloc_free_page (kpage);
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

bool stack_growth(void *vaddr){
  struct thread *cur = thread_current();
  void* frame = NULL;
  frame = palloc_get_page(PAL_USER | PAL_ZERO); // allocate a page from a USER_POOL, and add an entry to frame_table
  if(frame == NULL)
    return 0;
  else{
    //add the page to the process's address space
    if(!pagedir_set_page(cur->pagedir, pg_round_down(vaddr), frame, true)){
      //free the frame - set failure
      palloc_free_page(frame);
      return 0;
    }
  }
  return 1;
}
