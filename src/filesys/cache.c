#include "filesys/cache.h"
#include "threads/synch.h"
#include <list.h>

struct list $;
struct lock $_lock;

void cache_init(void) {
  list_init(&$);
}

void cache_push(struct cache_entry *ce) {
  lock_acquire(&$_lock);
  list_push_back(&$, ce);
  lock_release(&$_lock);
}

struct cache_entry *cache_pop(void) {
  struct cache_entry *ce = NULL;
  lock_acquire(&$_lock);
  ce = list_pop_front(&$);
  lock_release(&$_lock);
  return ce;
}