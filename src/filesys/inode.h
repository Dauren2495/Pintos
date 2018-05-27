#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"
#include "lib/kernel/list.h"

struct inode_disk
  {
    /* OLD: */
    /* block_sector_t start;               /\* First data sector. *\/ */
    /* off_t length;                       /\* File size in bytes. *\/ */
    /* unsigned magic;                     /\* Magic number. *\/ */
    /* uint32_t unused[125];               /\* Not used. *\/ */

    /* NEW: */
    //pointers to sectors where content is located
    block_sector_t direct[123]; //size of whole struct must be 128*4 bytes
    //pointers to pointers
    block_sector_t indirect;
    //pointers to pointers to pointers
    block_sector_t d_indirect; 
    unsigned magic;
    off_t length;
    bool is_dir; //true if directory, false if file
  };
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };



struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
