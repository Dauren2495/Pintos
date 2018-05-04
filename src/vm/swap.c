#include "threads/palloc.h"
#include <bitmap.h>
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "devices/block.h"
#include "vm/page.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#define BLOCK_PER_PG (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init(struct swap *swap)
{
  struct block *block = block_get_role(BLOCK_SWAP);
  size_t b_size = block_size(block);
  swap->block = block;
  size_t page_cnt  = b_size / BLOCK_PER_PG;
  swap->bitmap = bitmap_create(page_cnt);
  //lock_init(&swap->lock);
  sema_init(&swap->sema, 1);
}

void swap_write(struct swap *swap, uint8_t *upage)
{
  sema_down(&swap->sema);
  size_t page_idx = bitmap_scan_and_flip(swap->bitmap, 0, 1, false);
  sema_up(&swap->sema);
  size_t block_idx = page_idx * BLOCK_PER_PG;
  // update supplemental page table
  struct page *p =  page_lookup(upage);
  if(!p){
    p = calloc(sizeof(struct page), 1);
    p->upage =  upage;
    hash_insert(&thread_current()->pages, &p->hash_elem);
  }
  p->writable = true;
  p->swap = true;
  p->ofs = block_idx;
  // write page to swap
  for(size_t i = block_idx; i < block_idx + BLOCK_PER_PG; i++)
    {
      block_write(swap->block, i, upage);
      upage += BLOCK_SECTOR_SIZE;
    }
}
void swap_read(struct swap *swap, struct page *p)
{
  size_t block_idx = p->ofs;
  if(p->ofs % BLOCK_PER_PG != 0)
    printf("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\n");
  size_t page_idx = p->ofs / BLOCK_PER_PG;
  uint8_t *kpage = p->kpage;
  for(size_t i = block_idx; i < block_idx + BLOCK_PER_PG; i++)
    {
      block_read(swap->block, i, kpage);
      kpage += BLOCK_SECTOR_SIZE;
    }
  bitmap_set(swap->bitmap, page_idx, false);
}
void swap_remove(struct swap *swap, struct hash *pages)
{
  struct hash_iterator i;
  hash_first(&i, pages);
  while(hash_next(&i)){
    struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
    if(p->swap)
      bitmap_set(swap->bitmap, p->ofs / BLOCK_PER_PG, false);
  }
}
