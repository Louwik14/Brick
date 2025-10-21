#pragma once
#include <stdint.h>

/* Shim MIDI côté apps : mapping canal 1..16 -> status bytes, helpers NOTE ON/OFF et CC123.
   Ne dépend d'aucun header RTOS/core ; émission via hook midi_tx3(b0,b1,b2).
   Si l'appli ne fournit pas midi_tx3, un fallback no-op est utilisé (link OK côté host). */

#ifndef MIDI_HELPERS_CLAMP
#define MIDI_HELPERS_CLAMP(x,lo,hi) ((uint8_t)((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x))))
#endif

/* Hook d'émission : 3 octets MIDI. L'app/firmware peut fournir sa propre implémentation.
   Fallback no-op pour lier côté host si rien n’est fourni. */
__attribute__((weak)) void midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2);
static inline void midi_tx3_weak_impl(uint8_t b0, uint8_t b1, uint8_t b2) { (void)b0; (void)b1; (void)b2; }
static inline void _midi_tx3(uint8_t b0, uint8_t b1, uint8_t b2) {
  if ((void*)&midi_tx3) midi_tx3(b0,b1,b2); else midi_tx3_weak_impl(b0,b1,b2);
}

/* Helpers API (canal 1..16) */
static inline void midi_note_on(uint8_t ch1_16, uint8_t note, uint8_t vel) {
  uint8_t ch = MIDI_HELPERS_CLAMP(ch1_16, 1, 16) - 1;
  _midi_tx3((uint8_t)(0x90u | ch), note, vel);
}
static inline void midi_note_off(uint8_t ch1_16, uint8_t note, uint8_t vel) {
  uint8_t ch = MIDI_HELPERS_CLAMP(ch1_16, 1, 16) - 1;
  _midi_tx3((uint8_t)(0x80u | ch), note, vel);
}
static inline void midi_all_notes_off(uint8_t ch1_16) {
  uint8_t ch = MIDI_HELPERS_CLAMP(ch1_16, 1, 16) - 1;
  _midi_tx3((uint8_t)(0xB0u | ch), 123u, 0u); /* CC123 All Notes Off */
}

/* Usage:
   midi_note_on(3, 60, 100);
   midi_note_off(3, 60, 64);
   midi_all_notes_off(3);
*/
