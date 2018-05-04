#include <stdio.h>
#include <list.h>
#include "threads/synch.h"

struct list frame_table;
struct lock frame_table_lock;

void frame_init(void) {
	lock_init(&frame_table_lock);
	list_init(&frame_table);
}

void push_frame(struct frame_entry fe) {
	lock_acquire(&frame_table_lock);
	list_push_back(&frame_table, &fe->elem);
	lock_release(&frame_table_lock);
}

struct frame_entry *pop_frame(void) {
	lock_acquire(&frame_table_lock);
	struct frame_entry *fe = list_pop_front(&frame_table);
	lock_release(&frame_table_lock);
	return fe;
}