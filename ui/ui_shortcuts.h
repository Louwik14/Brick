/**
 * @file ui_shortcuts.h
 * @brief Raccourcis (SHIFT+...), gestion MUTE/PMUTE et ouverture des overlays.
 * @ingroup ui
 */

#ifndef BRICK_UI_SHORTCUTS_H
#define BRICK_UI_SHORTCUTS_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_input.h"  /* pour ui_input_event_t */

#ifdef __cplusplus
extern "C" {
#endif

void ui_shortcuts_init(void);
void ui_shortcuts_reset(void);

/**
 * @brief Tente de consommer un événement via le moteur de raccourcis.
 * @return true si consommé.
 */
bool ui_shortcuts_handle_event(const ui_input_event_t *evt);

/**
 * @brief Indique si le mode Keys (Keyboard) est actuellement actif
 *        (même si l’overlay n’est pas visible).
 *
 * @details
 * - Passe à true quand on entre sur l’overlay Keys.
 * - Reste true tant qu’on ne passe pas à un autre overlay (SEQ/ARP) ou qu’on ne
 *   change pas de cartouche via SHIFT+BM1..4.
 * - Reste true pendant MUTE ; à la sortie de MUTE, la LED est restaurée en conséquence.
 */
bool ui_shortcuts_is_keys_active(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_SHORTCUTS_H */
