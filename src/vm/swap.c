#include <list.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "threads/palloc.h"

static struct block *swap_block;
static struct lock swap_block_lock;
static struct list swap_table;
static struct lock swap_table_lock;
static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
static int allocate_index(void);

void swap_init(void) {
	swap_block = block_get_role(BLOCK_SWAP);
	list_init(&swap_table);
	lock_init(&swap_block_lock);
	lock_init(&swap_table_lock);
}

/* If frame is full, we choose the evition and swap it in DISK. We use the FIFO algorithm */
void* swap_out(enum palloc_flags flags){

	//Choose the frame evicted following the FIFO algoritm(Swap out)
	struct frame_entry *fe = pop_frame();
  
  struct swap_entry *se = (struct swap_entry *)calloc(1, sizeof(struct swap_entry));
  struct thread *t = thread_current();

  se->frame = fe->frame;
  fe->pe->location = DISK;
  se->pe = fe->pe;
  se->owner = fe->owner;

  //push the frame in the swap_table
	lock_acquire(&swap_block_lock);
  se->index = allocate_index();
  write_block((void *)fe->frame, se->index);
	lock_release(&swap_block_lock);
  push_swap(se);
  
  //free the original frame which choose as the evition
  pagedir_clear_page(t->pagedir, fe->pe->vaddr);
  palloc_free_page(fe->frame);
  free(fe);

  //And allocate the new_page
  void* page = palloc_get_page(flags);
  
  return page;
}

void swap_in(void* frame, struct page_entry *pe){
	struct swap_entry *se = lookup_swap(pe->vaddr);
	read_block((void *)frame, se->index);
  insert_frame_table(frame, pe);
  lock_acquire(&swap_table_lock);
  list_remove(&se->elem);
  lock_release(&swap_table_lock);
  free(se);
}

void read_block(void *frame, int index) {
	int i;
	lock_acquire(&swap_block_lock);
	for (i = 0; i < 8; i++) {
		block_read(swap_block, index + i, (void *)frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_block_lock);
}

void write_block(void *frame, int index) {
	int i;
	for (i = 0; i < 8; i++) {
		block_write(swap_block, index + i, (void *)frame + (i * BLOCK_SECTOR_SIZE));
	}
}

void push_swap(struct swap_entry *se) {
	lock_acquire(&swap_table_lock);
	list_push_back(&swap_table, &se->elem);
	lock_release(&swap_table_lock);
}

struct swap_entry *pop_swap(void) {
	lock_acquire(&swap_table_lock);
	struct swap_entry *se = list_entry(list_pop_front(&swap_table), struct swap_entry, elem);
	lock_release(&swap_table_lock);
	return se;
}

struct swap_entry *lookup_swap(void* vaddr){
  struct swap_entry *se = NULL;
  struct swap_entry *found = NULL;
  struct list_elem *e;

  lock_acquire(&swap_table_lock);
  for(e = list_begin(&swap_table); e != list_end(&swap_table); e  = list_next(e)){
  	se = list_entry(e, struct swap_entry, elem);
  	if(se->pe->vaddr == vaddr){
  		found = se;
  		break;
  	}
  }
  lock_release(&swap_table_lock);

  return found;
}

static int allocate_index(void){
	int index = 0;
	struct list_elem *e;
	struct swap_entry *se = NULL;
	lock_acquire(&swap_table_lock);
	list_sort(&swap_table, &sort, NULL);
	for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		se = list_entry(e, struct swap_entry, elem);
		if(se->index == index){
			index += 8;
			continue;
		}
		break;
	}
	lock_release(&swap_table_lock);
	return index;
}

static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	struct swap_entry *se_a = list_entry(a, struct swap_entry, elem);
	struct swap_entry *se_b = list_entry(b, struct swap_entry, elem);
	return se_a->index < se_b->index;
}
