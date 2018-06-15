#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

/* For proj.#2 about SYNC */
/* struct lock filesys_lock; */

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();

  cache_init();

  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  // for Proj.#4
  cache_flush();
}

static bool filesys_dir_lookup(struct dir *dir, const char *name, struct inode **target_inode, struct dir **target_dir, char *target_name) {
  const char *delimiter = "/";
  char *dirname = malloc(strlen(name) + 1);
  strlcpy(dirname, name, strlen(name) + 1);
  char *d, *save_ptr;
  bool exists = false;
  int depth = 0;
  int depth_index = 0;
  struct inode *inode;
  static char prev[NAME_MAX + 1];

  // if(dirname[0] == '/' || !thread_current()->dir){
  //   dir = dir_open_root();
  //   free(dirname);
  //   *target_dir = dir;
  //   *target_inode = file_get_inode((struct file *)dir);
  //   return dir != NULL;
  // }
  // else{
  //   printf("else!!!!!!\n");
  //   dir = dir_reopen(thread_current()->dir);
  // }
  for (d = strtok_r(dirname, delimiter, &save_ptr); d != NULL; d = strtok_r(NULL, delimiter, &save_ptr)) {
    if (strlen(d) > 14)
      return false;
    depth++;
  }
  strlcpy(dirname, name, strlen(name) + 1);
  for (d = strtok_r(dirname, delimiter, &save_ptr); d != NULL; d = strtok_r(NULL, delimiter, &save_ptr)) {
    depth_index++;
    if (depth_index == depth) {
      exists = true;
      break;
    }
    if (!strcmp(d, "."))
      continue;
    if (!strcmp(d, "..")) {
      if (prev[0] == '\0')
        break;
      strlcpy(d, prev, NAME_MAX + 1);
    }
    if (!dir_lookup(dir, d, &inode)){
      break;
    }
    // if (depth_index != 1)
    //   dir_close(dir);
    // dir = dir_open(inode);
    if(inode_is_dir(inode)) {
      dir_close(dir);
      dir = dir_open(inode);
    }
    else{
      inode_close(inode);
    }
    ////////////////////////
    strlcpy(prev, d, NAME_MAX + 1);
  }
  if(!exists){
    dir = NULL;
    inode = NULL;
  }
  strlcpy(target_name, d, strlen(d) + 1);
  free(dirname);
  *target_dir = dir;
  *target_inode = inode;

  return dir != NULL;
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_file) 
{
  // printf("filesys_create(): name(%s)\n", name);
  if (!strcmp(name, ""))
    return false;
// Proj.#4
  block_sector_t inode_sector = 0;
  // struct dir *root = thread_current()->dir;
  // if (!root)
  //   root = thread_current()->dir = dir_open_root();
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir)
    root = dir_open_root();
  else
    root = dir_reopen(thread_current()->dir);
  //////////////////////////////////////////////////
  struct dir *dir;
  struct inode *inode;
  char target[NAME_MAX + 1];
  if(!filesys_dir_lookup(root, name, &inode, &dir, target)){
    return false;
  }

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && (is_file? inode_create(inode_sector, initial_size, 0) : dir_create(inode_sector, 200))
                  // && dir_add (dir, name, inode_sector));
                  && dir_add (dir, target, inode_sector));
//
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  ///////////////
  dir_close (dir);
  ///////////////

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (!strcmp(name, ""))
    return NULL;
// Proj.#4
  // struct dir *root = thread_current()->dir;
  // if (!root)
  //   root = thread_current()->dir = dir_open_root();
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir)
    root = dir_open_root();
  else
    root = dir_reopen(thread_current()->dir);
  //////////////////////////////////////////////////
  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX + 1];
  if(!strcmp(name, "/")){
    dir = dir_open_root();
    if (root == dir)
      dir_close(dir);
    return (struct file *)root;
  }
  filesys_dir_lookup(root, name, &inode, &dir, target);
//  
  // if (dir != NULL){
  //   dir_lookup (dir, target, &inode);
  // }
  if (dir != NULL) {
    if (!strcmp(target, "..")) {
      inode = dir_parent_inode(dir);
      if (!inode) {
        return NULL;
      }
    } else if ((dir_is_root(dir) && strlen(target) == 0) || !strcmp(target, ".")) {
      return (struct file *) dir;
    } else
      dir_lookup (dir, target, &inode);
  }

  dir_close (dir);
  if (!inode){
    return NULL;
  }
  if (inode_is_dir(inode))
    return (struct file *)dir_open(inode);

  return file_open (inode);
}

struct dir *filesys_dir_open(const char *name) {
  // struct dir *root = thread_current()->dir;
  // if (!root)
  //   root = thread_current()->dir = dir_open_root();
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir)
    root = dir_open_root();
  else
    root = dir_reopen(thread_current()->dir);
  //////////////////////////////////////////////////
  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX + 1];
  filesys_dir_lookup(root, name, &inode, &dir, target);

  if (dir != NULL){
    dir_lookup(dir, target, &inode);
  }
  dir_close(dir);
  if (!inode){
    return NULL;
  }

  return dir_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  if (!strcmp(name, "/"))
    return false;
  // struct dir *root = thread_current()->dir;
  // if (!root)
  //   root = thread_current()->dir = dir_open_root();
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir)
    root = dir_open_root();
  else
    root = dir_reopen(thread_current()->dir);
  //////////////////////////////////////////////////
  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX + 1];
  if(!filesys_dir_lookup(root, name, &inode, &dir, target)){
    return false;
  }
  // if (dir != NULL)
  //   dir_lookup(dir, target, &inode);

  bool success = dir != NULL && dir_remove (dir, target);
  ///////////////
  dir_close (dir); 
  ///////////////

  return success;
}

bool filesys_chdir(const char *name) {
  if (!strcmp(name, "/")) {
    if(thread_current()->dir)
      dir_close(thread_current()->dir);
    thread_current()->dir = dir_open_root();
    return true;
  }
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir)
    root = dir_open_root();
  else
    root = dir_reopen(thread_current()->dir);
  //////////////////////////////////////////////////
  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX + 1];
  if(!filesys_dir_lookup(root, name, &inode, &dir, target)){
    return false;
  }

  if(!strcmp(target, "..")){
    inode = dir_parent_inode(dir);
    if (!inode)
      return false;
  } else if (!strcmp(target, ".") || (strlen(target)==0 && dir_is_root(dir))) {
    thread_current()->dir = dir;
    return true;
  } else
    dir_lookup(dir, target, &inode);

  dir_close(dir);
  dir = dir_open(inode);
  if (!dir)
    return false;
  else{
    dir_close(thread_current()->dir);
    thread_current()->dir = dir;
    return true;
  }
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  // if (!dir_create (ROOT_DIR_SECTOR, 16))
  if (!dir_create (ROOT_DIR_SECTOR, 50))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
