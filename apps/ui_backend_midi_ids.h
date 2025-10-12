/**
 * @file ui_backend_midi_ids.h
 * @brief Espace d'identifiants (local 13 bits) pour les évènements MIDI routés via ui_backend.
 * @ingroup ui
 *
 * @details
 * Convention de routage MIDI via ui_backend_param_changed():
 * - NOTE ON  : id = UI_DEST_MIDI | (0x0100 + note[0..127]), val = velocity[1..127]
 * - NOTE OFF : id = UI_DEST_MIDI | (0x0200 + note[0..127]), val = 0
 * - PANIC    : id = UI_DEST_MIDI | 0x0001,                val = 0
 *
 * L’objectif est de faire passer toutes les notes par ui_backend, pour que ton
 * futur **séquenceur** intercepte ce flux (live rec) au même endroit.
 */

#ifndef BRICK_UI_BACKEND_MIDI_IDS_H
#define BRICK_UI_BACKEND_MIDI_IDS_H

#include <stdint.h>
#include "ui_backend.h" /* UI_DEST_MIDI, UI_DEST_ID(...) */

#ifdef __cplusplus
extern "C" {
#endif

/* Bases locales (13 bits) */
#define UI_MIDI_NOTE_ON_BASE_LOCAL     0x0100u
#define UI_MIDI_NOTE_OFF_BASE_LOCAL    0x0200u
#define UI_MIDI_ALL_NOTES_OFF_LOCAL    0x0001u

/* Helpers pour composer l'identifiant complet (avec destination MIDI) */
#define UI_MIDI_NOTE_ON_ID(n)   (uint16_t)(UI_DEST_MIDI | ((UI_MIDI_NOTE_ON_BASE_LOCAL  + ((n) & 0x7Fu)) & 0x1FFFu))
#define UI_MIDI_NOTE_OFF_ID(n)  (uint16_t)(UI_DEST_MIDI | ((UI_MIDI_NOTE_OFF_BASE_LOCAL + ((n) & 0x7Fu)) & 0x1FFFu))
#define UI_MIDI_ALL_NOTES_OFF_ID (uint16_t)(UI_DEST_MIDI | (UI_MIDI_ALL_NOTES_OFF_LOCAL & 0x1FFFu))

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_BACKEND_MIDI_IDS_H */
