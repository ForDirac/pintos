#include <hash.h>

struct swap_entry {
	bool valid;
	void *page;
	void *frame;
	struct hash_elem elem;
};

bool swap_init(struct hash *h);
void set_swap_entry(struct hash *h, void *page, void *frame);