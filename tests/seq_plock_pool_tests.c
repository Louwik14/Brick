#include <assert.h>

#include "core/seq/seq_plock_pool.h"

int main(void) {
  seq_plock_pool_reset();
  assert(seq_plock_pool_capacity() >= 32);

  uint16_t offset = 0;
  assert(seq_plock_pool_alloc(16, &offset) == 0);
  assert(seq_plock_pool_used() == 16);

  const seq_plock_entry_t *entry0 = seq_plock_pool_get(offset, 0);
  (void)entry0;

  uint16_t offset2 = 0;
  assert(seq_plock_pool_alloc(0xFFFF, &offset2) == -1);

  return 0;
}

