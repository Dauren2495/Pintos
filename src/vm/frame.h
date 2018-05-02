#ifndef FRAME_H
#define FRAME_H

#include <hash.h>

struct frame{
  struct hash_elem hash_elem;
  struct list_elem list_elem;
  long long age;
  uint32_t *pd;
  void *upage;
  uint8_t *addr;
};

unsigned frame_hash(const struct hash_elem *e, void *aux);
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
void frame_free(const struct hash_elem *e, void *aux);
struct frame *frame_lookup(struct hash *frames, const uint8_t *addr);
void print_all_frames(const struct hash *hash);
void print_clock_list(struct list *list);
void *frame_evict(struct list* list,  int page_cnt);

#endif
