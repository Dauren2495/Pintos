#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "lib/random.h"


extern struct swap swap;

unsigned frame_hash(const struct hash_elem *e, void* aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  return hash_int(f->kpage); //hash_bytes(f->kpage, sizeof(f->kpage));
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct frame *f1 = hash_entry(a, struct frame, hash_elem);
  const struct frame *f2 = hash_entry(b, struct frame, hash_elem);
  return f1->kpage < f2->kpage;
}

void frame_free(const struct hash_elem *e, void *aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  free(f);
}

struct frame* frame_lookup(struct hash *frames, const uint8_t *kpage)
{
  struct frame f;// = calloc(sizeof(struct frame), 1);
  struct hash_elem *e;
  f.kpage = (uint32_t) kpage & ~PGMASK;
  e = hash_find(frames, &f.hash_elem);
  //free(f);
  return e != NULL ? hash_entry(e, struct frame, hash_elem) : NULL;
}

void print_all_frames(const struct hash *hash)
{
  printf("size of hash is %d\n", hash_size(hash));
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, hash);
  while(hash_next(&i)){
    struct frame *f = hash_entry(hash_cur(&i), struct frame, hash_elem);
    printf("frame at address %x\n", f->kpage);
  }
}

void *frame_evict(struct hash *frames, int page_cnt)
{
  void *kpage =  NULL;
  //lock_acquire(&swap.lock);
  struct hash_iterator i;
  hash_first(&i, frames);
  while(!kpage){
    while(hash_next(&i))
      {
	struct frame *f = hash_entry(hash_cur(&i), struct frame, hash_elem);
	struct page *p = page_lookup(f->hash, f->upage);
	if(pagedir_is_accessed(f->pd, f->upage))
	  pagedir_set_accessed(f->pd, f->upage, false);
	else{
	  printf("--TID: %d ------EVICTION PAGE -> TID: %d ->%x ------------\n", \
		 thread_current()->tid, f->tid, f->upage);
	   if(p->writable)
	    swap_write(&swap, f);
	  else
	    pagedir_clear_page(f->pd, f->upage);
	  kpage = (void*)f->kpage;
	  hash_delete(frames, &f->hash_elem);
	  //lock_release(&swap.lock);
	  free(f);
	  break;
	  }
      }
    if(!kpage)
      hash_first(&i, frames);
  }
  return kpage;
}

