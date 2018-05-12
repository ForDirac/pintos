#include <list.h>
#include "threads/thread.h"

struct frame_entry {
	uint32_t* frame;
	struct thread* owner;
	struct page_entry* pe;
	struct list_elem elem;
};

void frame_init(void);
void insert_frame_table(void* kpage, struct page_entry *pe);
void push_frame(struct frame_entry *fe);
struct frame_entry *pop_frame(void);
