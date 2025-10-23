#include "core/seq/seq_plock_pool.h"

#include <assert.h>

#ifndef SEQ_FEATURE_PLOCK_POOL_STORAGE
#define SEQ_FEATURE_PLOCK_POOL_STORAGE 1
#endif

#ifndef SEQ_PLOCK_POOL_CAPACITY_TEST
#define SEQ_PLOCK_POOL_CAPACITY_TEST 0u
#endif

#if SEQ_FEATURE_PLOCK_POOL_STORAGE

#ifndef SEQ_PLOCK_POOL_CAPACITY_FIRMWARE
#define SEQ_PLOCK_POOL_CAPACITY_FIRMWARE \
  (SEQ_MAX_TRACKS * SEQ_STEPS_PER_TRACK * SEQ_MAX_PLOCKS_PER_STEP)
#endif

_Static_assert(SEQ_PLOCK_POOL_CAPACITY_FIRMWARE <= 65535,
               "pool capacity exceeds 16-bit offset");

static seq_plock_entry_t s_pool[SEQ_PLOCK_POOL_CAPACITY_FIRMWARE];
static uint32_t s_used = 0;

void seq_plock_pool_reset(void) { s_used = 0; }

uint32_t seq_plock_pool_capacity(void) {
  return (uint32_t)(sizeof(s_pool) / sizeof(s_pool[0]));
}

uint32_t seq_plock_pool_used(void) { return s_used; }

int seq_plock_pool_alloc(uint16_t count, uint16_t *out_offset) {
  if (!out_offset) {
    return -1;
  }
  uint32_t capacity = seq_plock_pool_capacity();
  if ((uint32_t)count > (capacity - s_used)) {
    return -1;
  }
  *out_offset = (uint16_t)s_used;
  s_used += count;
  return 0;
}

seq_plock_entry_t *seq_plock_pool_get(uint16_t offset, uint16_t i) {
  uint32_t idx = (uint32_t)offset + (uint32_t)i;
  assert(idx < seq_plock_pool_capacity());
  return &s_pool[idx];
}

#elif SEQ_PLOCK_POOL_CAPACITY_TEST > 0

static seq_plock_entry_t s_pool[SEQ_PLOCK_POOL_CAPACITY_TEST];
static uint32_t s_used = 0;

void seq_plock_pool_reset(void) { s_used = 0; }

uint32_t seq_plock_pool_capacity(void) {
  return (uint32_t)(sizeof(s_pool) / sizeof(s_pool[0]));
}

uint32_t seq_plock_pool_used(void) { return s_used; }

int seq_plock_pool_alloc(uint16_t count, uint16_t *out_offset) {
  if (!out_offset) {
    return -1;
  }
  uint32_t capacity = seq_plock_pool_capacity();
  if ((uint32_t)count > (capacity - s_used)) {
    return -1;
  }
  *out_offset = (uint16_t)s_used;
  s_used += count;
  return 0;
}

seq_plock_entry_t *seq_plock_pool_get(uint16_t offset, uint16_t i) {
  uint32_t idx = (uint32_t)offset + (uint32_t)i;
  assert(idx < seq_plock_pool_capacity());
  return &s_pool[idx];
}

#else

void seq_plock_pool_reset(void) {}

uint32_t seq_plock_pool_capacity(void) { return 0; }

uint32_t seq_plock_pool_used(void) { return 0; }

int seq_plock_pool_alloc(uint16_t count, uint16_t *out_offset) {
  (void)count;
  (void)out_offset;
  return -1;
}

seq_plock_entry_t *seq_plock_pool_get(uint16_t offset, uint16_t i) {
  (void)offset;
  (void)i;
  return (seq_plock_entry_t *)0;
}

#endif

