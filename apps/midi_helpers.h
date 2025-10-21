#ifndef APPS_MIDI_HELPERS_H
#define APPS_MIDI_HELPERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Usage rapide:
 *   1. Inclure "apps/midi_helpers.h" dans le module qui doit émettre du MIDI.
 *   2. Fournir une implémentation forte de midi_tx3() pour router les octets vers l'I/O voulue.
 *   3. Appeler midi_note_on()/midi_note_off()/midi_all_notes_off() avec un canal 1..16.
 * Exemple: midi_note_on(3, 60, 100); // NOTE_ON (C4) sur le canal 3.
 */

/*
 * Hook d'émission MIDI.
 * Fournir une implémentation forte dans l'application (UART, USB, stub host, ...).
 * Si absent, l'édition de liens fournit un symbole faible nul et les helpers deviennent no-op.
 */
extern void midi_tx3(uint8_t status, uint8_t data1, uint8_t data2) __attribute__((weak));

static inline uint8_t midi_helpers_channel_index(uint8_t ch1_16)
{
    if (ch1_16 <= 1U) {
        return 0U;
    }
    if (ch1_16 >= 16U) {
        return 15U;
    }
    return (uint8_t)(ch1_16 - 1U);
}

static inline void midi_helpers_emit(uint8_t status, uint8_t data1, uint8_t data2)
{
    if (midi_tx3 != 0) {
        midi_tx3(status, data1, data2);
    }
}

/*
 * Émet un message NOTE ON.
 * Paramètres: canal 1..16 (clampé), note 0..127, vélocité 0..127.
 */
static inline void midi_note_on(uint8_t ch1_16, uint8_t note, uint8_t vel)
{
    const uint8_t channel = midi_helpers_channel_index(ch1_16);
    const uint8_t status = (uint8_t)(0x90U | channel);
    midi_helpers_emit(status, note, vel);
}

/*
 * Émet un message NOTE OFF.
 * Paramètres: canal 1..16 (clampé), note 0..127, vélocité 0..127.
 */
static inline void midi_note_off(uint8_t ch1_16, uint8_t note, uint8_t vel)
{
    const uint8_t channel = midi_helpers_channel_index(ch1_16);
    const uint8_t status = (uint8_t)(0x80U | channel);
    midi_helpers_emit(status, note, vel);
}

/*
 * Émet Control Change 123 "All Notes Off".
 * Paramètres: canal 1..16 (clampé).
 */
static inline void midi_all_notes_off(uint8_t ch1_16)
{
    const uint8_t channel = midi_helpers_channel_index(ch1_16);
    const uint8_t status = (uint8_t)(0xB0U | channel);
    midi_helpers_emit(status, 123U, 0U);
}

#ifdef __cplusplus
}
#endif

#endif /* APPS_MIDI_HELPERS_H */
