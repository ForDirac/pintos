#include <stdio.h>
#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"

struct list frame_table;
struct lock frame_table_lock;

void frame_init(void) {
	lock_init(&frame_table_lock);
	list_init(&frame_table);
}

void insert_frame_table(void* kpage, struct page_entry *pe){
	struct frame_entry *new_fe = (struct frame_entry *)calloc(1, sizeof(struct frame_entry));
	// struct frame_entry *new_fe = (struct frame_entry *)malloc(sizeof(struct frame_entry));
	struct thread *cur = thread_current();
	new_fe->frame = kpage;
	new_fe->owner = cur;
	new_fe->pe = pe;
	push_frame(new_fe);
}

void table_free_frame(void *kpage) {
	lock_acquire(&frame_table_lock);
	struct frame_entry *fe = lookup_frame(kpage);
	if (!fe)
		return;
	list_remove(&fe->elem);
	palloc_free_page(kpage);
	free(fe);
	lock_release(&frame_table_lock);
}

struct frame_entry *lookup_frame(void *kpage) {
	struct thread *t = thread_current();
	struct list_elem *e;
	struct frame_entry *fe = NULL;
	struct frame_entry *found = NULL;
	lock_acquire(&frame_table_lock);
	for (e = list_begin(&frame_table); e != list_end(&frame_table); e = list_next(e)) {
		fe = list_entry(e, struct frame_entry, elem);
		if (kpage == fe->frame && t == fe->owner) {
			found = fe;
			break;
		}
	}
	lock_release(&frame_table_lock);
	return found;
}

void push_frame(struct frame_entry *fe) {
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &fe->elem);
	lock_release(&frame_table_lock);
}

struct frame_entry *pop_frame(void) {
	lock_acquire(&frame_table_lock);
	struct frame_entry *fe = list_entry(list_pop_front(&frame_table), struct frame_entry, elem);
	lock_release(&frame_table_lock);
	return fe;
}
