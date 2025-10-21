#pragma once

#include <stdint.h>

#include "midi.h"

static inline uint8_t midi_channel_from_track_index(uint8_t track_index) {
    return (uint8_t)(((track_index % 16U) + 1U));
}

static inline uint8_t midi_zero_based_channel(uint8_t channel) {
    return (uint8_t)((channel > 0U ? channel - 1U : 0U) & 0x0FU);
}

static inline void midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    midi_note_on(MIDI_DEST_BOTH, midi_zero_based_channel(channel), note, velocity);
}

static inline void midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity) {
    midi_note_off(MIDI_DEST_BOTH, midi_zero_based_channel(channel), note, velocity);
}
