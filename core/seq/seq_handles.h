#pragma once
#include <stdint.h>

typedef struct {
  uint8_t bank;
  uint8_t pattern;
  uint8_t track;
} seq_track_handle_t;

typedef struct {
  uint8_t bank;
} seq_project_handle_t;

typedef struct {
  uint32_t tick;
} seq_cursor_t;
