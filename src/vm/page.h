#include <hash.h>


struct page_entry {
	void *page;
	void *vaddr;
	void *frame;
	struct hash_elem elem;
}

bool page_init(struct hash *h);
bool new_page(uint32_t *pd, void *vaddr, bool user, bool writable);