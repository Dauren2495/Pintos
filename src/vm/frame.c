#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "lib/random.h"


extern long long total_ticks;
extern struct swap swap;
extern size_t user_base;
unsigned frame_hash(const struct hash_elem *e, void* aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  return hash_bytes(&f->kpage, sizeof f->age);
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
  struct frame *f = calloc(sizeof(struct frame), 1);
  struct hash_elem *e;
  f->kpage = (uint32_t) kpage & ~PGMASK;
  e = hash_find(frames, &f->hash_elem);
  free(f);
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
void print_clock_list(struct list *list)
{
  struct list_elem *e;
  int j = 0;
  for(e = list_begin(list); e != list_end(list); e = list_next(e))
  {
    struct frame *f  = list_entry(e, struct frame, list_elem);
    printf("Element %d with age %lu\n", ++j, f->age);
  }
}
void *frame_evict(struct hash *frames, int page_cnt)
{
  int random;
  random_bytes(&random, sizeof(int));
  size_t size = hash_size(frames);
  random %= size;
  uint8_t *kpage = user_base + random * PGSIZE;
  struct frame *f = frame_lookup(frames, kpage);
  if(pagedir_is_dirty(f->pd, f->upage))
      swap_write(&swap, f->upage);
  pagedir_clear_page(f->pd, f->upage);
  
  /*struct list_elem *e;
  void *kpage =  NULL;
  while(!kpage){
    for(e = list_begin(list); e !=  list_end(list); e = list_next(e))
      {
	struct frame *f = list_entry(e, struct frame, list_elem);
	if(pagedir_is_accessed(f->pd, f->upage))
	  pagedir_set_accessed(f->pd, f->upage, false);
	else{
	  list_remove(&f->list_elem);
	  printf("Evicting Page %x\n", f->upage);
	  if(pagedir_is_dirty(f->pd, f->upage)){
	    printf("writing to  swap\n");
	    swap_write(&swap, f->upage);
	  }
	  pagedir_clear_page(f->pd, f->upage);
	  kpage = (void*)f->kpage;
	  break;
	  }
      }
  }
  */
  return kpage;
}

