#include <list.h>


#define PHYS 0
#define DISK 1
#define FILE 2

struct page_entry {
	uint32_t* vaddr;
	bool dirty;
	bool access;
	struct list_elem elem;
	int location;
};

void page_init(struct list *list);
bool new_page(void* vaddr, bool user, bool writable);
bool reclamation(struct page_entry *pe, bool user, bool writable);
void free_page(void *vaddr);
struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr);
