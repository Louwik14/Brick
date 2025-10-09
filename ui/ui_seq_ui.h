/**
 * @file ui_seq_ui.h
 * @brief Spécification UI interne du mode SEQ (mode principal + sous-mode SETUP).
 * @ingroup ui_modes
 *
 * @details
 * Ce module déclare deux cartouches virtuelles internes :
 *  - seq_ui_spec        : affichage/édition des pages principales du SEQ (All, Voix1–4)
 *  - seq_setup_ui_spec  : affichage/édition des pages de configuration (General, MIDI)
 *
 * Les paramètres sont adressés en espace interne UI (UI_DEST_UI),
 * donc aucun envoi vers les cartouches réelles n’est effectué.
 */

#ifndef BRICK_UI_SEQ_UI_H
#define BRICK_UI_SEQ_UI_H

#include "ui_spec.h"
#include "ui_backend.h" /* Pour les macros UI_DEST_* utilisées dans dest_id */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Cartouche virtuelle du mode SEQ (pages principales). */
extern const ui_cart_spec_t seq_ui_spec;

/** @brief Cartouche virtuelle du sous-mode SETUP du SEQ. */
extern const ui_cart_spec_t seq_setup_ui_spec;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_SEQ_UI_H */
