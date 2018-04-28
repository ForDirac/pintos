#include <stdio.h>
#include <hash.h>
#include "vm/swap.h"


// auxiliary functions
static unsigned hash_func(const struct hash_elem *e, void *aux NULL);
static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL);


bool swap_init(struct hash *h) {
	return hash_init(h, hash_func, less_func, NULL);
}

static unsigned hash_func(const struct hash_elem *e, void *aux NULL) {
	struct swap_entry *se = hash_entry(e, struct swap_entry, elem);
	return hash_bytes((unsigned)se->page, sizeof(unsigned));
}

static bool less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux NULL) {
	struct swap_entry *pe_a = hash_entry(a, struct swap_entry, elem);
	struct swap_entry *pe_b = hash_entry(b, struct swap_entry, elem);
	return pe_a->page < pe_b->page;
}


void set_swap_entry(struct hash *h, void *page, void *frame) {
	struct swap_entry se;
	se.valid = 1;
	se.page = page;
	se.frame = frame;
	struct hash_elem *existing = hash_insert(h, &se->elem);
	if (existing)
		hash_replace(h, &se->elem);
}