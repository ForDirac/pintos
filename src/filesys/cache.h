#include "devices/block.h"
#include <list.h>

struct cache_elem {
    struct block *block;
    block_sector_t sector;
    const void *buffer;
    struct list_elem elem;
};

void cache_init(void);
