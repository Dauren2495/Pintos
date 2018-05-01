#include <hash.h>

struct frame{
  struct hash_elem hash_elem;
  uint32_t *pd;
  void *upage;
  uint8_t *addr;
};

unsigned frame_hash(const struct hash_elem *e, void *aux);
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
void frame_free(const struct hash_elem *e, void *aux);
struct frame *frame_lookup(struct hash *frames, const uint8_t *addr);
void print_all_frames(const struct hash *hash);
