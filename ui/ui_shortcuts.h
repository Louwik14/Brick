/**
 * @file ui_shortcuts.h
 * @brief Raccourcis (SHIFT, MUTE/PMUTE), overlays (SEQ/ARP/KEY), et routage SEQ (pages, pads).
 * @ingroup ui
 *
 * @details
 * Objectifs (Elektron-like) :
 * - Tap court = Quick Step / Quick Clear.
 * - Maintien (un ou plusieurs steps) = **Preview P-Lock** (affichage des valeurs P-Lock,
 *   encodeurs modifient les P-Lock des steps maintenus). Aucune couleur "focus violet".
 * - À la relâche de tous les steps, fin de preview et retour à l’état normal.
 *
 * Invariants :
 * - MUTE prioritaire ; pas de dépendances circulaires ; zéro régression Keyboard/MIDI.
 */

#ifndef BRICK_UI_SHORTCUTS_H
#define BRICK_UI_SHORTCUTS_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_input.h"  /* ui_input_event_t */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise/Reset le moteur de raccourcis. */
void ui_shortcuts_init(void);
void ui_shortcuts_reset(void);

/**
 * @brief Tente de consommer un événement via le moteur de raccourcis.
 * @return true si consommé (l’événement ne doit plus être routé ailleurs).
 */
bool ui_shortcuts_handle_event(const ui_input_event_t *evt);

/**
 * @brief Indique si le contexte Keys (Keyboard) est actif
 *        (même si l’overlay n’est pas visible).
 */
bool ui_shortcuts_is_keys_active(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_SHORTCUTS_H */
