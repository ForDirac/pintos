#include "devices/block.h"
#include "threads/synch.h"

static struct block *swap_block;
static struct lock swap_block_lock;

void swap_init(void) {
	swap_block = block_get_role(BLOCK_SWAP);
	lock_init(&swap_block_lock);
}

void read_block(uint8_t *frame, int index) {
	int i;
	for (i = 0; i < 8; ++i) {
		block_read(swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
}

void write_block(uint8_t *frame, int index) {
	int i;
	for (i = 0; i < 8; ++i) {
		block_read(swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
}