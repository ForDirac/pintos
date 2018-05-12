#include <list.h>
#include "threads/thread.h"

struct swap_entry
{
	uint32_t* frame;
	struct thread* owner;
	struct page_entry* pe;
	struct list_elem elem;
	int index;
};

void swap_init(void);
void* swap_out(void);
void swap_in(void *frame);
void read_block(uint8_t *frame, int index);
void write_block(uint8_t *frame, int index);
void push_swap(struct swap_entry *se);
struct swap_entry *pop_swap(void);
struct swap_entry *lookup_swap(void* frame);
