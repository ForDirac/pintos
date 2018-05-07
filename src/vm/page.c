#include <stdio.h>
#include <list.h>
#include <bitmap.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"

// static unsigned hash_func(const struct hash_elem *e, void *aux NULL);
// static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL);
static bool install_page(void *upage, void *kpage, bool writable);
static void locate_page(void *vaddr);
static struct page_entry *lookup_page(uint32_t *vaddr);


bool page_init(struct list *list) {
	return list_init(list);
}

// static unsigned hash_func(const struct hash_elem *e, void *aux NULL) {
// 	struct page_entry *pe = hash_entry(e, struct page_entry, elem);
// 	return hash_bytes((unsigned)pe->vaddr, sizeof(unsigned));
// }

// static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL) {
// 	struct page_entry *pe_a = hash_entry(a, struct page_entry, elem);
// 	struct page_entry *pe_b = hash_entry(b, struct page_entry, elem);
// 	return pe_a->vaddr < pe_b->vaddr;
// }


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
  locate_page(vaddr);
  // Reset page table
  success = install_page(upage, kpage, writable);
  printf("install_page result in new_page: %s\n", success ? "SUCCESS" : "FAILURE");  // for debugging
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

static void locate_page(void *vaddr) {
	struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
	struct page_entry *pe = (struct page_entry *)malloc(sizeof(struct page_entry));
	pe.vaddr = vaddr;
	pe.dirty = !!(vaddr & PTE_D);
	pe.access = !!(vaddr & PTE_A);
	list_push_back(page_table, &pe.elem);
}

static struct page_entry *lookup_page(uint32_t *vaddr) {
  void *upage = pg_round_down(vaddr);
  struct thread *t = thread_current();
  struct list *page_table = &t->sup_page_table;
  struct list_elem *e;
  struct page_entry *pe;
  for (e = list_begin(page_table); e != list_end(page_table); e = list_next(e)) {
    pe = list_entry(e, struct page_entry, elem);
    if (upage == pg_round_down(pe->vaddr))
      break;
  }
  return pe;
}