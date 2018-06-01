#include "filesys/cache.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include <list.h>
#include <string.h>
#include <stdio.h>

struct list cache_list;
// struct lock cache_lock;

static void periodic_flush(void *aux UNUSED);

void cache_init(void) {
  list_init(&cache_list);
	thread_create("_flusher", 0, periodic_flush, NULL);
}

static void periodic_flush(void *aux UNUSED){
	while(1){
		timer_sleep(5*TIMER_FREQ);
		cache_flush();
	}
}

void cache_push(struct cache_entry *ce) {
	if(list_size(&cache_list) == 64)
		cache_pop();
	lock_init(&ce->lock);
  list_push_back(&cache_list, &ce->elem);
}

void cache_pop(void) {
	// lock_acquire(&cache_lock);
  struct cache_entry *ce = NULL;
  ce = list_entry(list_pop_front(&cache_list), struct cache_entry, elem);
  cache_block_write(ce);
  free(ce);
	// lock_release(&cache_lock);
}

void cache_flush(void){
	struct list_elem *e;
	struct cache_entry *ce = NULL;
  for (e = list_begin(&cache_list); e != list_end(&cache_list); e = list_next(e)){
    ce = list_entry(e, struct cache_entry, elem);
    cache_block_write(ce);
	}
}

void cache_block_read(struct cache_entry *ce){
	block_read(ce->block, ce->sector, ce->buffer);
}

void cache_block_write(struct cache_entry *ce){
	if(ce->dirty){
		block_write(ce->block, ce->sector, ce->buffer);
		ce->dirty = 0;
	}
}

void read_cache(struct block *block, block_sector_t sector, void *buffer){
	// lock_acquire(&cache_lock);	
	struct cache_entry *ce = lookup_cache(block, sector);
	if(!ce){
		ce = (struct cache_entry *)calloc(1, sizeof(struct cache_entry));
		ce->block = block;
		ce->sector = sector;
		cache_block_read(ce);
		// block_read(block, sector, ce->buffer);
		cache_push(ce);
		ce->dirty = 0;
	}
	memcpy(buffer, ce->buffer, BUF_SIZE);
	// lock_release(&cache_lock);
}

void write_cache(struct block *block, block_sector_t sector, const void *buffer){
	// lock_acquire(&cache_lock);
	struct cache_entry *ce = lookup_cache(block, sector);

	if(!ce){
		ce = (struct cache_entry *)calloc(1, sizeof(struct cache_entry));
		ce->block = block;
		ce->sector = sector;
		cache_push(ce);
		ce->dirty = 0;
	}
	memcpy(ce->buffer, buffer, BUF_SIZE);
	ce->dirty = 1;
	cache_block_write(ce);
	// lock_release(&cache_lock);
}

struct cache_entry *lookup_cache(struct block *block, block_sector_t sector){
	struct list_elem *e;
	struct cache_entry *ce = NULL;
  for (e = list_begin(&cache_list); e != list_end(&cache_list); e = list_next(e)) {
    ce = list_entry(e, struct cache_entry, elem);
    if(ce->block == block && ce->sector == sector){
    	return ce;
    }
  }
  return NULL;
}
