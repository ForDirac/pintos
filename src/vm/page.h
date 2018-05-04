#include <hash.h>


struct page_entry {
	uint32_t* vaddr;
	bool dirty;
	bool access;
	struct list_elem elem;
}

bool page_init(struct hash *h);
bool new_page(void *vaddr, bool user, bool writable);
void free_page(void *vaddr);