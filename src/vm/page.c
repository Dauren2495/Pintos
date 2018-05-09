#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

unsigned page_hash(const struct hash_elem *e, void* aux)
{
  const struct page *p = hash_entry(e, struct page, hash_elem);
  return hash_bytes(&p->upage, sizeof p->upage);
  
}
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct page *p1 = hash_entry(a, struct page, hash_elem);
  const struct page *p2 = hash_entry(b, struct page, hash_elem);
  return p1->upage < p2->upage;
}
void page_free(const struct hash_elem *e, void *aux)
{
  const struct page *p = hash_entry(e, struct page, hash_elem);
  free(p);
}
struct page* page_lookup(struct hash *hash, const uint8_t *upage)
{
  struct page *p = calloc(sizeof(struct page), 1);
  struct hash_elem *e;
  p->upage = (uint32_t) upage & ~PGMASK;
  e = hash_find(hash, &p->hash_elem);
  free(p);
  return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

void print_all_pages(const struct hash *hash)
{
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, hash);
  while(hash_next(&i)){
    struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
    printf("Element %d with address %x\n", ++j, p->upage);
  }
}

void remove_frames(const struct hash *pages, const struct hash *frames)
{
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, pages);
  while(hash_next(&i)){
    struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
    struct frame *f = frame_lookup(frames, p->kpage);
    if(f != NULL){
      palloc_free_page(f->kpage);
    }
  }
}
