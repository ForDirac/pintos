#include <list.h>
#include "filesys/file.h"


#define PHYS 0
#define DISK 1
#define FILE 2

#define ALL_ZERO 0
#define EXE_FILE 1

struct file_info
{
	struct file *file;
	off_t ofs;
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
};

struct page_entry {
	uint32_t* vaddr;
	bool dirty;
	bool access;
	struct list_elem elem;
	int location;
	bool lazy_loading;
	struct file_info file_info;
};

void page_init(struct list *list);
struct page_entry *locate_page(void *vaddr, int location);
bool *locate_lazy_page(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool lazy_load_segment(struct page_entry *new_entry);
bool new_page(void *vaddr, bool user, bool writable);
bool reclamation(void *vaddr, bool user, bool writable);
void free_page(void *vaddr);
struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr);
