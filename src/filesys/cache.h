#include "devices/block.h"
#include <list.h>

struct cache_entry {
    struct block *block;
    block_sector_t sector;
    const void *buffer;
    struct list_elem elem;
};

void cache_init(void);
void cache_push(struct cache_entry *ce);
struct cache_entry *cache_pop(void);
