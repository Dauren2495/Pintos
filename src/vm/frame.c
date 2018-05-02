#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

extern long long total_ticks;
extern struct swap swap;

unsigned frame_hash(const struct hash_elem *e, void* aux)
{
  const struct frame *f = hash_entry(e, struct frame, hash_elem);
  return hash_bytes(&f->addr, sizeof f->age);
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  const struct frame *f1 = hash_entry(a, struct frame, hash_elem);
  const struct frame *f2 = hash_entry(b, struct frame, hash_elem);
  if(f1->age == f2->age)
    return f1->addr < f2->addr;
  else
    return f1->age < f2->age;
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
  printf("size of hash is %d\n", hash_size(hash));
  struct hash_iterator i;
  int j = 0;
  hash_first(&i, hash);
  while(hash_next(&i)){
    struct frame *f = hash_entry(hash_cur(&i), struct frame, hash_elem);
    printf("Element %d with age %lu\n", ++j, f->age);
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
void *frame_evict(struct list *list, int page_cnt)
{
  struct list_elem *e;
  struct frame *f = list_entry(list_begin(list), struct frame, list_elem);
  list_remove(&f->list_elem);
  printf("Evicting Page %x\n", f->upage);
  if(pagedir_is_dirty(f->pd, f->upage)){
    printf("writing to  swap\n");
    swap_write(&swap, f->upage);
  }
  /*for(e = list_begin(list); e != list_end(list); e = list_next(e))
  {
    f  = list_entry(e, struct frame, list_elem);
    uint32_t *pte = lookup_page(f->pd, f->upage, false);
    if(!(*pte & PTE_W)){
      if(*pte & PTE_A){
	*pte &= ~(uint32_t) PTE_A;
	f->age = total_ticks;
	list_remove(&f->list_elem);
	list_push_back(list, &f->list_elem); 
      }
      else{
	list_remove(&f->list_elem);
	*pte = 0;
	break;
	//evict
	//change age and move back
	// change pd
	//chnage upage
      }
    }
    }*/
  return (void*)f->addr;
  
}

