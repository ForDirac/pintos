#include <list.h>
#include "filesys/file.h"


#define PHYS 0
#define DISK 1
#define FILE 2

#define ALL_ZERO 0
#define EXE_FILE 1

struct page_entry {
	uint32_t* vaddr;
	bool dirty;
	bool access;
	struct list_elem elem;
	int location;
	bool lazy_loading;
	int lazy_type;
	struct file *file;
};

void page_init(struct list *list);
struct page_entry *locate_page(void *vaddr, int location);
struct page_entry *locate_lazy_page(void *vaddr, int lazy_type, struct file *file);
bool lazy_load_segment(void *vaddr, bool user, bool writable, int lazy_type, struct file *file);
bool new_page(void *vaddr, bool user, bool writable);
bool reclamation(void *vaddr, bool user, bool writable);
void free_page(void *vaddr);
struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr);
