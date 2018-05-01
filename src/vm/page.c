#include "vm/page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

unsigned page_hash(const struct hash_elem *e, void* aux)
{
  const struct page *p = hash_entry(e, struct page, hash_elem);
  return hash_bytes(&p->addr, sizeof p->addr);
  
}
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct page *p1 = hash_entry(a, struct page, hash_elem);
  const struct page *p2 = hash_entry(b, struct page, hash_elem);
  return p1->addr < p2->addr;
}
void page_free(const struct hash_elem *e, void *aux)
{
  const struct page *p = hash_entry(e, struct page, hash_elem);
  free(p);
}
struct page* page_lookup(const uint8_t *addr)
{
  struct page p;
  struct hash_elem *e;
  uint32_t mask = (uintptr_t) addr & ~PGMASK;
  p.addr = (uint8_t*) mask;
  e = hash_find(&thread_current()->pages, &p.hash_elem);
  return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}
void print_all_pages(const struct hash *hash)
{
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, hash);
  while(hash_next(&i)){
    struct page *p = hash_entry(hash_cur(&i), struct page, hash_elem);
    printf("Element %d with address %x\n", ++j, p->addr);
  }
}
