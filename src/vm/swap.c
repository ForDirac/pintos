#include <list.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/malloc.h"
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

void* swap_out(enum palloc_flags flags){
	struct frame_entry *fe = pop_frame(); //FIFO, Eviction
  struct swap_entry *se = (struct swap_entry *)calloc(1, sizeof(struct swap_entry));
  // struct swap_entry *se = (struct swap_entry *)malloc(sizeof(struct swap_entry));
  se->frame = fe->frame;
  fe->pe->location = DISK;
  se->pe = fe->pe;
  se->owner = fe->owner;
  se->index = allocate_index();
  write_block((uint8_t*)fe->frame, se->index);
  push_swap(se);
  palloc_free_page(fe->frame);
  free(fe);
  return palloc_get_page(flags);
  // return se->frame;
}

void swap_in(void* frame, struct page_entry *pe){
	struct swap_entry *se = lookup_swap(frame);
	read_block((uint8_t*)frame, se->index);
  insert_frame_table(frame, pe);
  lock_acquire(&swap_table_lock);
  list_remove(&se->elem);
  lock_release(&swap_table_lock);
  free(se);
}

void read_block(uint8_t *frame, int index) {
	int i;
	lock_acquire(&swap_block_lock);
	for (i = 0; i < 8; i++) {
		block_read(swap_block, index + i, (uint8_t *)frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_block_lock);
}

void write_block(uint8_t *frame, int index) {
	int i;
	lock_acquire(&swap_block_lock);
	for (i = 0; i < 8; i++) {
		block_write(swap_block, index + i, (uint8_t *)frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_block_lock);
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

static int allocate_index(void){
	int index = 0;
	struct list_elem *e;
	struct swap_entry *se = NULL;
	lock_acquire(&swap_table_lock);
	list_sort(&swap_table, &sort, NULL);
	for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		se = list_entry(e, struct swap_entry, elem);
		// printf("allocate_index se->index: %d\n", se->index);
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

struct swap_entry *lookup_swap(void* frame){
  struct swap_entry *se = NULL;
  struct swap_entry *found = NULL;
  struct list_elem *e;

  lock_acquire(&swap_table_lock);
  for(e = list_begin(&swap_table); e != list_end(&swap_table); e  = list_next(e)){
  	se = list_entry(e, struct swap_entry, elem);
  	if(se->frame == frame){
  		found = se;
  		break;
  	}
  }
  lock_release(&swap_table_lock);

  return found;
}
