static bool
load_segment_modified (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  printf("Loading segment from executable\n");
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      printf("one full page read\n");
      //   Calculate how to fill this page.
      //   We will read PAGE_READ_BYTES bytes from FILE
      //   and zero the final PAGE_ZERO_BYTES bytes.
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      struct page *p = calloc(sizeof(struct page), 1);
      p->file = file;
      p->ofs = ofs;
      p->addr = upage;
      p->read_bytes = page_read_bytes;
      p->zero_bytes = page_zero_bytes;
      p->writable = writable;

      hash_insert(&pages, &p->hash_elem);
      
      // Advance. 
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}
