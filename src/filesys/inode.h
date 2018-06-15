#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

// bool check_alloc (struct inode_disk *disk_inode);
void inode_init (void);
bool inode_create (block_sector_t, off_t, int);
// bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
// off_t grow_inode(struct inode *inode, off_t length);
// size_t add_indirect_block(struct inode *inode, size_t n_sectors);
// size_t add_dindirect_block(struct inode *inode, size_t n_sectors);
// size_t add_ddindirect_block(struct inode *inode, size_t n_sectors, struct indirect_block *i_block);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);
// For Proj.#4
bool inode_is_dir(struct inode *inode);

#endif /* filesys/inode.h */
