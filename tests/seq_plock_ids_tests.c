#include <assert.h>

#include "core/seq/seq_plock_ids.h"

int main(void) {
  for (int v = -128; v <= 127; ++v) {
    uint8_t u = pl_u8_from_s8((int8_t)v);
    int8_t s = pl_s8_from_u8(u);
    assert(s == (int8_t)v);
  }
  assert(pl_is_midi(0x00) && pl_is_midi(0x3F));
  assert(pl_is_cart(0x40) && pl_cart_id(0x40) == 0);
  return 0;
}

