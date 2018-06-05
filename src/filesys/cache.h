#include "devices/block.h"
#include "threads/synch.h"
#include <list.h>

#define BUF_SIZE 512

struct cache_entry {
    struct block *block;
    struct lock lock;
    block_sector_t sector;
    bool dirty;
    char buffer[BUF_SIZE];
    struct list_elem elem;
};

void cache_init(void);
void cache_push(struct cache_entry *ce);
void cache_pop(void);
void cache_flush(void);
void cache_block_read(struct cache_entry *ce);
void cache_block_write(struct cache_entry *ce);
void read_cache(struct block *block, block_sector_t sector, void *buffer);
void write_cache(struct block *block, block_sector_t sector, const void *buffer);
struct cache_entry *lookup_cache(struct block *block, block_sector_t sector);
