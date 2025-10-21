#pragma once
#include <stdint.h>

typedef struct {
  uint8_t note;
  uint8_t vel;
  uint16_t length;
  int8_t micro;
  uint8_t flags;
} seq_step_view_t;

enum {
  SEQ_STEPF_HAS_VOICE       = 1u << 0,
  SEQ_STEPF_HAS_ANY_PLOCK   = 1u << 1,
  SEQ_STEPF_HAS_SEQ_PLOCK   = 1u << 2,
  SEQ_STEPF_HAS_CART_PLOCK  = 1u << 3,
  SEQ_STEPF_AUTOMATION_ONLY = 1u << 4,
  SEQ_STEPF_MUTED           = 1u << 5,
};

typedef struct {
  void *_opaque;
} seq_plock_iter_t;
