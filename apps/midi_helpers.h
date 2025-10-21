#pragma once

#include <stdint.h>

#include "midi.h"

static inline void midi_send_note_on(uint8_t channel_1based, uint8_t note, uint8_t velocity) {
    if ((channel_1based == 0U) || (channel_1based > 16U)) {
        return;
    }
    midi_note_on(MIDI_DEST_BOTH, (uint8_t)(channel_1based - 1U), note, velocity);
}

static inline void midi_send_note_off(uint8_t channel_1based, uint8_t note) {
    if ((channel_1based == 0U) || (channel_1based > 16U)) {
        return;
    }
    midi_note_off(MIDI_DEST_BOTH, (uint8_t)(channel_1based - 1U), note, 0U);
}

static inline void midi_send_all_notes_off(uint8_t channel_1based) {
    if ((channel_1based == 0U) || (channel_1based > 16U)) {
        return;
    }
    midi_all_notes_off(MIDI_DEST_BOTH, (uint8_t)(channel_1based - 1U));
}
