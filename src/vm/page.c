#include <stdio.h>
#include <hash.h>
#include <bitmap.h>
#include "vm/page.h"
#include "threads/threads.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"

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


bool new_page(uint32_t *pd, void *vaddr, bool user, bool writable) {
  void *page = pagedir_get_page(pd, vaddr);
  void *frame;
  bool success;
  // Obtain a frame to store the page
  if (user)
    frame = palloc_get_page(PAL_USER | PAL_ZERO);
  else
    frame = palloc_get_page(PAL_ZERO);
  // Locate the page that faulted in the supplemental page table.
  locate_page(void *page, void *vaddr, void *frame);
  success = pagedir_set_page(pd, page, frame, writable);
  printf("pagedir_set_page result in get_page: %s\n", success ? "SUCCESS" : "FAILURE");  // for debugging
  return success;
}

static void locate_page(void *page, void *vaddr, void *frame) {
	struct thread *t = thread_current();
	struct hash *page_table = &t->page_table;
	struct hash_elem *old;
	struct page_entry pe;
	pe.page = page;
	pe.vaddr = vaddr;
	pe.frame = frame;
	old = hash_insert(page_table, &pe.elem);
	if (old)
		hash_replace(page_table, &pe.elem);
}