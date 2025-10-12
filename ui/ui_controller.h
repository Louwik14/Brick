/**
 * @file ui_controller.h
 * @brief Logique centrale de contrôle de l’interface utilisateur Brick.
 * @ingroup ui
 *
 * @details
 * Traduit les **entrées** (boutons/encodeurs) en modifications d’état UI, selon
 * la spécification active (`ui_cart_spec_t`). Aucune I/O matérielle ici.
 */

#ifndef BRICK_UI_UI_CONTROLLER_H
#define BRICK_UI_UI_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_model.h"
#include "ui_spec.h"

void ui_init(const ui_cart_spec_t *spec);
void ui_switch_cart(const ui_cart_spec_t *spec);

const ui_state_t*     ui_get_state(void);
const ui_cart_spec_t* ui_get_cart(void);
const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index);

void ui_on_button_menu(int index);
void ui_on_button_page(int index);
void ui_on_encoder(int enc_index, int delta);

void ui_mark_dirty(void);
bool ui_is_dirty(void);
void ui_clear_dirty(void);

#endif /* BRICK_UI_UI_CONTROLLER_H */
