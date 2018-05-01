#include <stdio.h>
#include "vm/swap.h"
#include "devices/block.h"


static struct block* swap_block;


void swap_init() {
	swap_block = block_get_role(BLOCK_SWAP);
}

void read_block(block *block, uint8_t *frame, int index) {
	int i;
	for (i = 0; i < 8; ++i) {
		block_read(block, index + i, frame + (i*BLOCK_SECTOR_SIZE));
	}
}

void write_block(block *block, uint8_t *frame, int index) {
	int i;
	for (i = 0; i < 8; ++i) {
		block_write(block, index + i, frame + (i*BLOCK_SECTOR_SIZE));
	}
}