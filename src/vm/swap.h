#ifndef SWAP_H
#define SWAP_H

#include <bitmap.h>
#include <hash.h>
#include "devices/block.h"
#include "vm/page.h"
#include "threads/synch.h"

struct swap{
  struct bitmap *bitmap;
  struct block *block;
  struct lock lock;
  //struct semaphore sema;
};
void swap_init(struct swap *);
void swap_write(struct swap *,struct frame *);
void swap_read(struct swap *,struct page *);
void swap_remove(struct swap *, struct hash *);
// free swap of terminating process
#endif
  
