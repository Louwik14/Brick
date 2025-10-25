#pragma once
#include <stdint.h>

static inline uint8_t seq_midi_channel_for_track(uint16_t track_index) {
  uint16_t t = (track_index < 16U) ? track_index : 15U;
  return (uint8_t)(t + 1U);
}
