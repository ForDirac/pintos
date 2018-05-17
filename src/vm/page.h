#include <list.h>
#include "filesys/file.h"
#include "threads/thread.h"
#include "userprog/syscall.h"

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
	// lazy_loading
	bool lazy_loading;
	// mmap
	bool is_mmap;
	// common
	struct file *file;
	off_t offset;
	size_t page_zero_bytes;
	bool writable;
};

void page_init(struct list *list);
struct page_entry *locate_page(void *vaddr, int location);
struct page_entry *locate_lazy_page(void *vaddr, struct file *file, off_t offset, size_t page_zero_bytes, bool writable);
struct page_entry *locate_mmap_page(void *vaddr, struct file *file, off_t offset, size_t page_zero_bytes);
bool lazy_load_segment(void *vaddr, bool user, bool writable, struct file *file, off_t offset, size_t page_zero_bytes);
bool new_page(void *vaddr, bool user, bool writable);
bool reclamation(void *vaddr, bool user, bool writable);
void table_free_page(void *vaddr);
struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr);
