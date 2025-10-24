#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint8_t param_id;
  uint8_t value;
  uint8_t flags;
} seq_plock_entry_t;

#ifdef __cplusplus
extern "C" {
#endif

void seq_plock_pool_reset(void);
uint16_t seq_plock_pool_capacity(void);
uint16_t seq_plock_pool_used(void);
int seq_plock_pool_alloc(uint16_t count, uint16_t *out_offset);
seq_plock_entry_t* seq_plock_pool_get(uint16_t offset, uint16_t i);

#ifdef __cplusplus
}
#endif

