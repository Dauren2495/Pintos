#ifndef PAGE_H
#define PAGE_H

#include <hash.h>
#include "filesys/file.h"
#include "threads/malloc.h"

struct page{
  struct hash_elem hash_elem;
  bool swap;
  uint8_t* kpage;
  uint8_t* upage;
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
  bool lock;
};


unsigned page_hash(const struct hash_elem *, void*);
bool page_less(const struct hash_elem *, const struct hash_elem *, void *);
void page_free(const struct hash_elem *, void *);
struct page* page_lookup(struct hash *, const uint8_t *);
void print_all_pages(const struct hash *);
void remove_frames(const struct hash *, const struct hash *);

#endif

  
