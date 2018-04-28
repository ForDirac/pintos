#include <stdio.h>
#include <list.h>

struct list frame_table;

void frame_init(void) {
	list_init(&frame_table);
}