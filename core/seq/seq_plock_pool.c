#include "seq_config.h"
#include "seq_plock_pool.h"
#include <stdint.h>
#include <stddef.h>

#ifndef SEQ_FEATURE_PLOCK_POOL
#define SEQ_FEATURE_PLOCK_POOL 0
#endif

#ifndef SEQ_FEATURE_PLOCK_POOL_STORAGE
#define SEQ_FEATURE_PLOCK_POOL_STORAGE 0
#endif

#if SEQ_FEATURE_PLOCK_POOL_STORAGE

#ifndef SEQ_MAX_TRACKS
#error "SEQ_MAX_TRACKS is undefined. Include seq_config.h before seq_plock_pool.c"
#endif
#ifndef SEQ_STEPS_PER_TRACK
#error "SEQ_STEPS_PER_TRACK is undefined. Include seq_config.h before seq_plock_pool.c"
#endif
#ifndef SEQ_MAX_PLOCKS_PER_STEP
#error "SEQ_MAX_PLOCKS_PER_STEP is undefined. Include seq_config.h before seq_plock_pool.c"
#endif

#define SEQ_PLOCK_POOL_CAPACITY_FIRMWARE \
  ((uint32_t)SEQ_MAX_TRACKS * (uint32_t)SEQ_STEPS_PER_TRACK * (uint32_t)SEQ_MAX_PLOCKS_PER_STEP)

_Static_assert(SEQ_PLOCK_POOL_CAPACITY_FIRMWARE <= 65535u,
               "pool capacity exceeds 16-bit offset");

#define PLOCK_POOL_CAPACITY   (SEQ_PLOCK_POOL_CAPACITY_FIRMWARE)

#elif defined(SEQ_PLOCK_POOL_CAPACITY_TEST)

#if (SEQ_PLOCK_POOL_CAPACITY_TEST > 65535)
#error "SEQ_PLOCK_POOL_CAPACITY_TEST must fit into 16-bit offset"
#endif

#define PLOCK_POOL_CAPACITY   (SEQ_PLOCK_POOL_CAPACITY_TEST)

#else
#define PLOCK_POOL_CAPACITY   (0u)
#endif

#if (PLOCK_POOL_CAPACITY > 0)
static seq_plock_entry_t s_pool[PLOCK_POOL_CAPACITY];
#endif

static uint16_t s_used = 0;

void seq_plock_pool_reset(void) {
  s_used = 0;
}

uint16_t seq_plock_pool_used(void) {
  return s_used;
}

uint16_t seq_plock_pool_capacity(void) {
  return PLOCK_POOL_CAPACITY;
}

int seq_plock_pool_alloc(uint16_t n, uint16_t *off_out) {
  if (n == 0) {
    if (off_out) {
      *off_out = s_used;
    }
    return 0;
  }
  uint32_t need = (uint32_t)s_used + (uint32_t)n;
  if (need > (uint32_t)PLOCK_POOL_CAPACITY) {
    return -1;
  }
  if (off_out) {
    *off_out = s_used;
  }
  s_used = (uint16_t)need;
  return 0;
}

seq_plock_entry_t* seq_plock_pool_get(uint16_t off, uint16_t i) {
  uint32_t idx = (uint32_t)off + (uint32_t)i;
  if (idx >= (uint32_t)PLOCK_POOL_CAPACITY) {
    return NULL;
  }
#if (PLOCK_POOL_CAPACITY > 0)
  return &s_pool[idx];
#else
  return NULL;
#endif
}
