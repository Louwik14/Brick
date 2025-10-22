// apps/seq_midi_bridge.c
#include <stdint.h>
#include "midi/midi.h"   // pile matérielle (USB + DIN)

// -----------------------------------------------------------------------------
// Hook fort : appelé par les helpers apps/midi_helpers.h
// Transforme (b0,b1,b2) en appels bas-niveau hot-safe.
// -----------------------------------------------------------------------------
void midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2) {
    const uint8_t st = b0 & 0xF0;
    const uint8_t ch = b0 & 0x0F;

    switch (st) {
        case 0x90: // NOTE ON
            if (b2)
                midi_note_on(MIDI_DEST_BOTH, ch, b1, b2);
            else
                midi_note_off(MIDI_DEST_BOTH, ch, b1, 64);
            break;

        case 0x80: // NOTE OFF
            midi_note_off(MIDI_DEST_BOTH, ch, b1, b2 ? b2 : 64);
            break;

        case 0xB0: // Control Change (CC)
            midi_cc(MIDI_DEST_BOTH, ch, b1, b2);
            break;

        default:
            break;
    }
}
