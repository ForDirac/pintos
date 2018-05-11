#include <stdio.h>
#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/frame.h"
#include "vm/page.h"

struct list frame_table;
struct lock frame_table_lock;

void frame_init(void) {
	lock_init(&frame_table_lock);
	list_init(&frame_table);
}

void insert_frame_table(void* kpage, struct page_entry *pe){
	struct frame_entry *new_fe = (struct frame_entry *)malloc(sizeof(struct frame_entry));
	struct thread *cur = thread_current();
	new_fe->frame = kpage;
	new_fe->owner = cur;
	new_fe->pe = pe;
	push_frame(new_fe);
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