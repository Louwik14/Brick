#pragma once
#include <stdint.h>

typedef struct {
  uint8_t note;
  uint8_t vel;
  uint16_t length;
  int8_t micro;
  uint8_t flags;
} seq_step_view_t;

typedef struct {
  void *_opaque;
} seq_plock_iter_t;
