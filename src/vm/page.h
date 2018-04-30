#include <hash.h>
#include "filesys/file.h"

struct page{
  struct hash_elem hash_elem;
  uint8_t* addr;
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};


unsigned page_hash(const struct hash_elem *e, void* aux UNUSED);
void page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);


  
