#ifndef FRAME_H
#define FRAME_H

#include <hash.h>

struct frame{
  struct hash_elem hash_elem;
  struct list_elem list_elem;
  long long age;
  uint32_t *pd;
  uint8_t *upage;
  uint8_t *kpage;
};

unsigned frame_hash(const struct hash_elem *, void *);
bool frame_less(const struct hash_elem *, const struct hash_elem *, void *);
void frame_free(const struct hash_elem *, void *);
struct frame *frame_lookup(struct hash *, const uint8_t *);
void print_all_frames(const struct hash *);
void print_clock_list(struct list *);
void *frame_evict(struct hash* ,  int );

#endif
