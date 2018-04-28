#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"

struct frame_table_entry {
	uint32_t* frame;
	struct thread* owner;
	struct page_entry* aux;
	struct list_elem elem;
};

void frame_init(void);