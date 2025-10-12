/**
 * @file ui_keyboard_ui.h
 * @brief Déclaration de la vitrine UI du mode KEYBOARD.
 * @ingroup ui_modes
 */

#ifndef BRICK_UI_KEYBOARD_UI_H
#define BRICK_UI_KEYBOARD_UI_H

#include "ui_spec.h"
#include "ui_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Spécification du mode Keyboard (vitrine UI). */
extern const ui_cart_spec_t ui_keyboard_spec;

/** @brief Identifiant local du paramètre “Omnichord” (UI_DEST_UI). */
extern const uint16_t KBD_OMNICHORD_ID;

/** @brief Identifiant local (UI_DEST_UI) — page 2 — “Note order”. */
extern const uint16_t KBD_NOTE_ORDER_ID;

/** @brief Identifiant local (UI_DEST_UI) — page 2 — “Chord buttons override scale”. */
extern const uint16_t KBD_CHORD_OVERRIDE_ID;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_KEYBOARD_UI_H */
