#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

unsigned frame_hash(const struct hash_elem *e, void* aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  return hash_bytes(&f->addr, sizeof f->addr);
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct frame *f1 = hash_entry(a, struct frame, hash_elem);
  const struct frame *f2 = hash_entry(b, struct frame, hash_elem);
  return f1->addr < f2->addr;
}
void frame_free(const struct hash_elem *e, void *aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  free(f);
}
struct frame* frame_lookup(struct hash *frames, const uint8_t *addr)
{
  struct frame f;
  struct hash_elem *e;
  uint32_t mask = (uintptr_t) addr & ~PGMASK;
  f.addr = (uint8_t*) mask;
  e = hash_find(frames, &f.hash_elem);
  return e != NULL ? hash_entry(e, struct frame, hash_elem) : NULL;
}
void print_all_frames(const struct hash *hash)
{
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, hash);
  while(hash_next(&i)){
    struct frame *f = hash_entry(hash_cur(&i), struct frame, hash_elem);
    printf("Element %d with address %x\n", ++j, f->addr);
  }
}

