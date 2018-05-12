#include <list.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static struct block *swap_block;
static struct lock swap_block_lock;
static struct list swap_table;
static struct lock swap_table_lock;
static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux);
static int allocate_index(void);

void swap_init(void) {
	swap_block = block_get_role(BLOCK_SWAP);
	list_init(&swap_table);
	lock_init(&swap_block_lock);
	lock_init(&swap_table_lock);
}

void* swap_out(void){
	struct frame_entry *fe = pop_frame(); //FIFO, Eviction
  struct swap_entry *new_frame = (struct swap_entry *)malloc(sizeof(struct swap_entry));
  new_frame->frame = fe->frame;
  fe->pe->location = DISK;
  new_frame->pe = fe->pe;
  new_frame->owner = fe->owner;
  new_frame->index = allocate_index();
  write_block((uint8_t*)fe->frame, new_frame->index);
  push_swap(new_frame);
  free(fe);
  return new_frame->frame;
}

void swap_in(void* frame){
	struct swap_entry *se = lookup_swap(frame);
	read_block((uint8_t*)frame, se->index);
	struct swap_entry *se_old = pop_swap();
	free(se_old);
}

void read_block(uint8_t *frame, int index) {
	int i;
	lock_acquire(&swap_block_lock);
	for (i = 0; i < 8; ++i) {
		block_read(swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_block_lock);
}

void write_block(uint8_t *frame, int index) {
	int i;
	lock_acquire(&swap_block_lock);
	for (i = 0; i < 8; ++i) {
		block_write(swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
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
		printf("allocate_index se->index: %d\n", se->index);
		if(se->index == index){
			index += 8;
			continue;
		}
		break;
	}
	lock_release(&swap_table_lock);
	return index;
}

static bool sort(const struct list_elem *a, const struct list_elem *b, void *aux) {
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