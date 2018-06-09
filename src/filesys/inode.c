#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
// For Proj.#4
#define DIRECT_BLOCKS 10
// #define INDIRECT_BLOCK 1
// #define D_INDIRECT_BLOCK 1

#define BLOCK_NUMBER 12
#define INDIRECT_BLOCKS 128
#define INIT_SECTOR 0xffffffff

#define MAX_SIZE 8*1024*8192

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t direct_index;
    uint32_t indirect_index;
    uint32_t d_indirect_index;
    uint32_t unused[111];               /* Not used. */
    block_sector_t blocks[BLOCK_NUMBER];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

// Check the number of sectors in Indirect_Blocks
static size_t check_indirect_block (off_t size){
  if (size <= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS){
      return 0;
  }
  size -= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS;
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS);
}

// Check the Double_indirect_blocks
static size_t check_d_indirect_block (off_t size){
  if (size <= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS*1){
    return 0;
  }
  return 1;
}

// For Proj.#4
// static block_sector_t get_sector(struct inode *inode, off_t pos);

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    // struct inode_disk data;             /* Inode content. */
    // For Proj.#4
    off_t length;
    off_t read_length;
    uint32_t direct_index;
    uint32_t indirect_index;
    uint32_t d_indirect_index;
    struct lock i_lock;
    block_sector_t blocks[BLOCK_NUMBER];
  };

struct indirect_block{
  block_sector_t blocks[INDIRECT_BLOCKS];
};

bool check_alloc(struct inode_disk *disk_inode);
void check_dalloc(struct inode *inode);
void dalloc_indirect (block_sector_t *blocks, size_t remain_sectors);
void dalloc_d_indirect(block_sector_t *blocks, size_t indirect_block, size_t sectors);
off_t grow_inode(struct inode *inode, off_t length);
size_t add_indirect_block(struct inode *inode, size_t n_sectors);
size_t add_dindirect_block(struct inode *inode, size_t n_sectors);
size_t add_ddindirect_block(struct inode *inode, size_t n_sectors, struct indirect_block *i_block);

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t length, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < length){
    int idx;
    block_sector_t indirect_block[INDIRECT_BLOCKS];
    // in DIRECT_BLOCK size (0 ~ 10*512)
    if (pos < BLOCK_SECTOR_SIZE*DIRECT_BLOCKS){
      return inode->blocks[pos/BLOCK_SECTOR_SIZE];
    }
    // in INDIRECT_BLOCK size (10*512 ~ 512*128 + 10*512) 
    else if (pos < (BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS*1)){
      pos -= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS;
      // idx = pos/(BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS) + DIRECT_BLOCKS;
      
      // read the 11th block(double_indirect_block)
      read_cache(fs_device, inode->blocks[DIRECT_BLOCKS], &indirect_block);
      pos = pos%(BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS);
      return indirect_block[pos/BLOCK_SECTOR_SIZE];
    }
    // in DOUBLE_INDIRECT_BLOCK size (512*128 + 10*512 ~ 512*128*128 + 512*128 + 10*512) 
    else{
      // read the 12th block(double_indirect_block)
      read_cache(fs_device, inode->blocks[DIRECT_BLOCKS + 1], &indirect_block);
      pos -= (BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS*1);
      idx = pos/(BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS);
      // read the indirect_block in the 12th block(double_indirect_block)
      read_cache(fs_device, indirect_block[idx], &indirect_block);
      pos = pos%(BLOCK_SECTOR_SIZE*INDIRECT_BLOCKS);
      return indirect_block[pos/BLOCK_SECTOR_SIZE];
    }
  }
  else
    return -1;
}

bool check_alloc (struct inode_disk *disk_inode){
  struct inode inode;
  inode.length = 0;
  inode.direct_index = 0;
  inode.indirect_index = 0;
  inode.d_indirect_index = 0;

  grow_inode(&inode, disk_inode->length);
  disk_inode->direct_index = inode.direct_index;
  disk_inode->indirect_index = inode.indirect_index;
  disk_inode->d_indirect_index = inode.d_indirect_index;
  memcpy(&disk_inode->blocks, &inode.blocks, sizeof(block_sector_t)*BLOCK_NUMBER);
  return true;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      // size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      if (disk_inode->length > MAX_SIZE){
        disk_inode->length = MAX_SIZE;
      }
      disk_inode->magic = INODE_MAGIC;
      if(check_alloc(disk_inode)){
        write_cache(fs_device, sector, disk_inode);
        success = true;
      }
      // // memset(disk_inode->blocks, INIT_SECTOR, BLOCK_NUMBER * sizeof(block_sector_t));
      // if (free_map_allocate (sectors, &disk_inode->start)) 
      //   {
      //     // write_cache (fs_device, sector, disk_inode);
      //     write_cache(fs_device, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[BLOCK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           // write_cache (fs_device, disk_inode->start + i, zeros);
      //           write_cache(fs_device, disk_inode->start + i, zeros);
      //       }
      //     success = true; 
        // }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init(&inode->i_lock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  // write the inode in this inode_disk
  struct inode_disk inode_disk;
  read_cache(fs_device, inode->sector, &inode_disk);
  inode->direct_index = inode_disk.direct_index;
  inode->indirect_index = inode_disk.indirect_index;
  inode->d_indirect_index = inode_disk.d_indirect_index;
  inode->length = inode_disk.length;
  inode->read_length = inode_disk.length;
  memcpy(&inode->blocks, &inode_disk.blocks, sizeof(block_sector_t)*BLOCK_NUMBER);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0){
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed) {
        free_map_release (inode->sector, 1);
        check_dalloc(inode);
    }
    else {
      struct inode_disk disk_inode;
      disk_inode.direct_index = inode->direct_index;
      disk_inode.indirect_index = inode->indirect_index;
      disk_inode.d_indirect_index = inode->d_indirect_index;
      disk_inode.length = inode->length;
      disk_inode.magic = INODE_MAGIC;
      memcpy(&disk_inode.blocks, &inode->blocks, BLOCK_NUMBER*sizeof(block_sector_t));
      write_cache(fs_device, inode->sector, &disk_inode);
    }
    free (inode); 
  }
}

void check_dalloc(struct inode *inode){
  size_t sectors = bytes_to_sectors(inode->length);
  size_t indirect_block = check_indirect_block(inode->length);
  size_t d_indirect_block = check_d_indirect_block(inode->length);

  uint32_t idx = 0;

  // Dealloc the Direct_block's sectors
  while (sectors && idx < 10){
    free_map_release(inode->blocks[idx], 1);
    sectors --;
    idx ++;
  }

  // Dealloc the Indirect_block's sectors
  while (indirect_block && idx < 11){
    size_t remain_sectors = sectors < INDIRECT_BLOCKS ? sectors : INDIRECT_BLOCKS;
    dalloc_indirect(&inode->blocks[idx], remain_sectors);
    sectors -= remain_sectors;
    indirect_block --;
    idx++;
  }

  // Dealloc the Double_indirect_block's sectors
  if (d_indirect_block){
    dalloc_d_indirect(&inode->blocks[idx], indirect_block, sectors);
  }
}

void dalloc_indirect (block_sector_t *blocks, size_t remain_sectors){
  uint32_t n = 0;
  struct indirect_block i_block;
  read_cache(fs_device, *blocks, &i_block);
  while (n < remain_sectors){
    free_map_release(i_block.blocks[n], 1);
    n ++;
  }
  free_map_release(*blocks, 1);
}

void dalloc_d_indirect(block_sector_t *blocks, size_t indirect_block, size_t sectors){
  uint32_t n = 0;
  struct indirect_block i_block;
  read_cache(fs_device, *blocks, &i_block);
  while (n < indirect_block){
    size_t remain_sectors = sectors < INDIRECT_BLOCKS ? sectors : INDIRECT_BLOCKS;
    dalloc_indirect(&i_block.blocks[n], remain_sectors);
    sectors -= remain_sectors;
  }
  free_map_release(*blocks, 1);
}


/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  off_t length = inode->read_length;

  if (offset >= length){
    return bytes_read;
  }

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      // block_sector_t sector_idx = get_sector(inode, offset);
      block_sector_t sector_idx = byte_to_sector(inode, length, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          // read_cache (fs_device, sector_idx, buffer + bytes_read);
          read_cache(fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          // read_cache (fs_device, sector_idx, bounce);
          read_cache(fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if(offset + size > inode_length(inode)){
    lock_acquire(&inode->i_lock);
    inode->length = grow_inode(inode, offset + size);
    lock_release(&inode->i_lock);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      // block_sector_t sector_idx = get_sector(inode, offset);
      block_sector_t sector_idx = byte_to_sector (inode, inode_length(inode), offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          // write_cache (fs_device, sector_idx, buffer + bytes_written);
          write_cache (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            // read_cache (fs_device, sector_idx, bounce);
            read_cache(fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          // write_cache (fs_device, sector_idx, bounce);
          write_cache(fs_device, sector_idx, bounce);
        }
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

// For Proj.#4
// static block_sector_t get_sector(struct inode *inode, off_t pos) {
//   ASSERT (inode != NULL);
//   bool success;

//   if (inode->sector == 0)
//     return inode->data.start;

//   // in DIRECT_BLOCK size (0 ~ 10*512)
//   if (pos < BLOCK_SECTOR_SIZE*DIRECT_BLOCKS) {
//     block_sector_t sector = 0;
//     int idx = pos / BLOCK_SECTOR_SIZE;

//     // there is no DIRECT_BLOCK in correct position
//     if (inode->data.blocks[idx] == INIT_SECTOR) {
//       success = free_map_allocate(1, &sector);
//       if(!success && sector != 0){
//         free_map_release(sector, 1);
//         return -1;
//       }
//       inode->data.blocks[idx] = sector;
//       write_cache(fs_device, inode->sector, &inode->data);
//     }
//     // there is already DIRECT_BLOCKS in correct position 
//     else {
//       sector = inode->data.blocks[idx];
//     }
//     return sector;
//   }
//   // in INDIRECT_BLOCK size (10*512 ~ 512*128 + 10*512) 
//   else if (pos < BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + INDIRECT_BLOCKS*BLOCK_SECTOR_SIZE) {
//     pos -= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS;
//     block_sector_t sector = 0;

//     // there is no INDIRECT_BLOCK in 11th index
//     if (inode->data.blocks[DIRECT_BLOCKS] == INIT_SECTOR) {
//       block_sector_t indirect_inode_sector = 0;
//       block_sector_t indirect_sector = 0;
//       success = free_map_allocate(1, &indirect_inode_sector);
//       if (!success && indirect_inode_sector != 0){
//         free_map_release(indirect_inode_sector, 1);
//         return -1;
//       }
//       success = free_map_allocate(1, &indirect_sector);
//       if (!success && indirect_sector != 0) {
//         free_map_release(indirect_sector, 1);
//         return -1;
//       }
//       inode->data.blocks[DIRECT_BLOCKS] = indirect_inode_sector;
//       write_cache(fs_device, inode->sector, &inode->data);
//       write_cache(fs_device, indirect_inode_sector, &indirect_sector);
//       sector = indirect_sector;
//     }
//     // there is already INDIRECT_BLOCK in 11th index 
//     else {
//       block_sector_t indirect_inode_sector = inode->data.blocks[DIRECT_BLOCKS];
//       block_sector_t indirect_inode[BLOCK_SECTOR_SIZE];
//       block_sector_t indirect_sector = 0;
//       int idx = pos / BLOCK_SECTOR_SIZE;
//       read_cache(fs_device, indirect_inode_sector, indirect_inode);

//       // there is no DIRECT_BLOCK in correct position
//       if (indirect_inode[idx] == 0) {
//         success = free_map_allocate(1, &indirect_sector);
//         if (!success && indirect_sector != 0) {
//           free_map_release(indirect_sector, 1);
//           return -1;
//         }
//         indirect_inode[idx] = indirect_sector;
//         write_cache(fs_device, indirect_inode_sector, indirect_inode);
//       } 
//       // there is already DIRECT_BLOCK in correct position
//       else {
//         indirect_sector = indirect_inode[idx];
//       }
//       sector = indirect_sector;
//     }
//     return sector;
//   } 
//   // in DOUBLE_INDIRECT_BLOCK size (512*128 + 10*512 ~ 512*128*128 + 512*128 + 10*512) 
//   else if (pos < BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + INDIRECT_BLOCKS*BLOCK_SECTOR_SIZE + INDIRECT_BLOCKS*INDIRECT_BLOCKS*BLOCK_SECTOR_SIZE) {
//     pos -= BLOCK_SECTOR_SIZE*DIRECT_BLOCKS + INDIRECT_BLOCKS*BLOCK_SECTOR_SIZE;
//     block_sector_t sector = 0;

//     // there is no D_INDIRECT_BLOCK in 12th index 
//     if (inode->data.blocks[DIRECT_BLOCKS + 1] == INIT_SECTOR) {
//       block_sector_t d_indirect_inode_sector = 0;
//       block_sector_t indirect_inode_sector = 0;
//       block_sector_t indirect_sector = 0;
//       success = free_map_allocate(1, &d_indirect_inode_sector);
//       if (!success && d_indirect_inode_sector != 0) {
//         free_map_release(d_indirect_inode_sector, 1);
//         return -1;
//       }
//       success = free_map_allocate(1, &indirect_inode_sector);
//       if (!success && indirect_inode_sector != 0) {
//         free_map_release(indirect_inode_sector, 1);
//         return -1;
//       }
//       success = free_map_allocate(1, &indirect_sector);
//       if (!success && indirect_sector != 0) {
//         free_map_release(indirect_sector, 1);
//         return -1;
//       }
//       inode->data.blocks[DIRECT_BLOCKS + 1] = d_indirect_inode_sector;
//       write_cache(fs_device, inode->sector, &inode->data);
//       write_cache(fs_device, d_indirect_inode_sector, &indirect_inode_sector);
//       write_cache(fs_device, indirect_inode_sector, &indirect_sector);
//       sector = indirect_sector;
//       return sector;
//     }
//     // there is D_INDIRECT_BLOCK in 12th index 
//     else {
//       block_sector_t d_indirect_inode_sector = inode->data.blocks[DIRECT_BLOCKS + 1];
//       block_sector_t d_indirect_inode[BLOCK_SECTOR_SIZE];
//       block_sector_t indirect_inode_sector = 0;
//       block_sector_t indirect_sector = 0;
//       int idx = pos / BLOCK_SECTOR_SIZE;
//       int d_idx = idx / INDIRECT_BLOCKS;
//       int i_idx = idx % INDIRECT_BLOCKS - 1;
//       read_cache(fs_device, d_indirect_inode_sector, d_indirect_inode);

//       // there is no INDIRECT_BLOCK in correct position
//       if (d_indirect_inode[d_idx] == 0) {
//         success = free_map_allocate(1, &indirect_inode_sector);
//         if (!success && indirect_inode_sector != 0) {
//           free_map_release(indirect_inode_sector, 1);
//           return -1;
//         }
//         success = free_map_allocate(1, &indirect_sector);
//         if (!success && indirect_sector != 0) {
//           free_map_release(indirect_sector, 1);
//           return -1;
//         }
//         d_indirect_inode[d_idx] = indirect_inode_sector;
//         write_cache(fs_device, d_indirect_inode_sector, d_indirect_inode);
//         write_cache(fs_device, indirect_inode_sector, &indirect_sector);
//         sector = indirect_sector;

//       }
//       // there is INDIRECT_BLOCK in correct position 
//       else {
//         block_sector_t indirect_inode_sector = d_indirect_inode[d_idx];
//         block_sector_t indirect_inode[BLOCK_SECTOR_SIZE];
//         block_sector_t indirect_sector;
//         read_cache(fs_device, indirect_inode_sector, indirect_inode);

//         // there is no DIRECT_BLOCK in correct position
//         if (indirect_inode[i_idx] == 0) {
//           success = free_map_allocate(1, &indirect_sector);
//           if (!success && indirect_sector != 0) {
//             free_map_release(indirect_sector, 1);
//             return -1;
//           }
//           indirect_inode[i_idx] = indirect_sector;
//           write_cache(fs_device, indirect_inode_sector, indirect_inode);
//           sector = indirect_sector;
//         } 
//         // there is DIRECT_BLOCK in correct position
//         else {
//           indirect_sector = indirect_inode[i_idx];
//           sector = indirect_sector;
//         }
//       }
//       return sector;
//     }
//   } 
//   // exceed the offset
//   else {
//     return 0;
//   }
// }

off_t grow_inode(struct inode *inode, off_t length){
  static char zeros[BLOCK_SECTOR_SIZE];
  size_t n_sectors = bytes_to_sectors(length) - bytes_to_sectors(inode->length);

  if (n_sectors == 0)
    return length;

  while(inode->direct_index < 10){
    free_map_allocate(1, &inode->blocks[inode->direct_index]);
    write_cache(fs_device, inode->blocks[inode->direct_index], zeros);
    inode->direct_index ++;
    n_sectors --;
    if(n_sectors == 0)
      return length;
  }
  if (inode->direct_index == 10){
    n_sectors = add_indirect_block(inode, n_sectors);
    if(n_sectors == 0)
      return length;
  }
  if (inode->direct_index == 11){
    n_sectors = add_dindirect_block(inode, n_sectors);
  }
  return length - n_sectors*BLOCK_SECTOR_SIZE;
}

size_t add_indirect_block(struct inode *inode, size_t n_sectors){
  static char zeros[BLOCK_SECTOR_SIZE];
  struct indirect_block i_block;
  if (inode->indirect_index == 0)
    free_map_allocate(1, &inode->blocks[inode->direct_index]);
  else
    read_cache(fs_device, inode->blocks[inode->direct_index], &i_block);
  
  while (inode->indirect_index < INDIRECT_BLOCKS){
    free_map_allocate(1, &i_block.blocks[inode->indirect_index]);
    write_cache(fs_device, i_block.blocks[inode->indirect_index], zeros);
    inode->indirect_index ++;
    n_sectors --;
    if (n_sectors == 0)
      break;
  }

  write_cache(fs_device, inode->blocks[inode->direct_index], &i_block);
  if (inode->indirect_index == INDIRECT_BLOCKS){
    inode->indirect_index = 0;
    inode->direct_index ++;
  }

  return n_sectors;
}

size_t add_dindirect_block(struct inode *inode, size_t n_sectors){
  struct indirect_block i_block;
  if (inode->d_indirect_index == 0 && inode->indirect_index == 0)
    free_map_allocate(1, &inode->blocks[inode->direct_index]);
  else
    read_cache(fs_device, inode->blocks[inode->direct_index], &i_block);

  while (inode->indirect_index < INDIRECT_BLOCKS){
    n_sectors = add_ddindirect_block(inode, n_sectors, &i_block);

    if (n_sectors == 0)
      break;
  }

  write_cache(fs_device, inode->blocks[inode->direct_index], &i_block);
  return n_sectors;
}

size_t add_ddindirect_block(struct inode *inode, size_t n_sectors, struct indirect_block *i_block){
  static char zeros[BLOCK_SECTOR_SIZE];
  struct indirect_block d_block;
  if(inode->d_indirect_index == 0)
    free_map_allocate(1, &i_block->blocks[inode->indirect_index]);
  else
    read_cache(fs_device, i_block->blocks[inode->indirect_index], &d_block);

  while(inode->d_indirect_index < INDIRECT_BLOCKS){
    free_map_allocate(1, &d_block.blocks[inode->d_indirect_index]);
    write_cache(fs_device, d_block.blocks[inode->d_indirect_index], zeros);
    inode->d_indirect_index ++;
    n_sectors --;
    if (n_sectors == 0)
      break;
  }

  write_cache(fs_device, i_block->blocks[inode->indirect_index], &d_block);
  if(inode->d_indirect_index == INDIRECT_BLOCKS){
    inode->d_indirect_index = 0;
    inode->indirect_index ++;
  }

  return n_sectors;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
