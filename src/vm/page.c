#include <stdio.h>
#include <hash.h>
#include <bitmap.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

// pagedir_set_page()
// pagedir_get_page()
// pagedir_clear_page()
// palloc_get_multiple()
// palloc_free_multiple()
// bitmap.c
// hash.c
static unsigned hash_func(const struct hash_elem *e, void *aux NULL);
static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL);
static void locate_page(void *page, void *vaddr, void *frame);


bool page_init(struct hash *h) {
	return hash_init(h, hash_func, less_func, NULL);
}

static unsigned hash_func(const struct hash_elem *e, void *aux NULL) {
	struct page_entry *pe = hash_entry(e, struct page_entry, elem);
	return hash_bytes((unsigned)pe->page, sizeof(unsigned));
}

static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL) {
	struct page_entry *pe_a = hash_entry(a, struct page_entry, elem);
	struct page_entry *pe_b = hash_entry(b, struct page_entry, elem);
	return pe_a->page < pe_b->page;
}


bool new_page(void *vaddr, bool user, bool writable) {
	struct thread *t = thread_current();
  void *upage = pg_round_down(vaddr);
  void *kpage;
  bool success;
  // Obtain a frame to store the page
  if (user)
    kpage = (void *)palloc_get_page(PAL_USER | PAL_ZERO);
  else
    kpage = (void *)palloc_get_page(PAL_ZERO);
  if (!kpage) {
  	// swap and get kpage
  }
  // Locate the page that faulted in the supplemental page table.
  locate_page(upage, vaddr, kpage);
  // Reset page table
  success = install_page(upage, kpage, writable);
  printf("install_page result in new_page: %s\n", success ? "SUCCESS" : "FAILURE");  // for debugging
  return success;
}

static void locate_page(void *upage, void *vaddr, void *kpage) {
	struct thread *t = thread_current();
	struct hash_elem *old;
	struct page_entry pe; // TODO: malloc??
	pe.vaddr = vaddr;
	// TODO
	// pe.dirty = ;
	// pe.accessed = ;
	old = hash_insert(&t->sup_page_table, &pe.elem);
	if (old)
		hash_replace(&t->sup_page_table, &pe.elem);
}