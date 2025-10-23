#pragma once

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint8_t param_id;  // 0..255 (MIDI internes <0x40, Cart >=0x40)
  uint8_t value;     // 0..255 (s8 encodé via helpers si besoin)
  uint8_t flags;     // réservé (bit0=active, bit1=trig, etc.)
} seq_plock_entry_t;

#ifdef __cplusplus
extern "C" {
#endif

void     seq_plock_pool_reset(void);
uint32_t seq_plock_pool_capacity(void);
uint32_t seq_plock_pool_used(void);
int      seq_plock_pool_alloc(uint16_t count, uint16_t *out_offset);
const seq_plock_entry_t* seq_plock_pool_get(uint16_t offset, uint16_t i);

#ifdef __cplusplus
}
#endif

