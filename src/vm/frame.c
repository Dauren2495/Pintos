#include "vm/page.h"

unsigned page_hash(const struct hash_elem *e, void* aux UNUSED)
{
  const struct page *p = page_entry(e, struct page, hash_elem);
  return hash_bytes( &p->addr, sizeof p>addr);
}
void page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  const struct page *p1 = page_entry(a, struct page, hash_elem);
  const struct page *p2 = page_entry(b, struct page, hash_elem);
  return p1->addr > p2->addr;
}

