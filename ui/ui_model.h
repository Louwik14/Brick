/**
 * @file ui_model.h
 * @brief État mutable de l’interface utilisateur Brick (RAM UI).
 * @ingroup ui
 *
 * @details
 * Les dimensions du modèle sont alignées sur la source de vérité `ui_spec.h`.
 * On NE redéfinit PAS les macros de la spec. On crée des alias côté modèle.
 */

#ifndef BRICK_UI_UI_MODEL_H
#define BRICK_UI_UI_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_spec.h"   /* UI_MENUS_PER_CART, UI_PAGES_PER_MENU, UI_PARAMS_PER_PAGE */

/* ============================================================
 * Dimensions du modèle (alias, sans redéfinir la spec)
 * ============================================================ */
#define UI_MODEL_MAX_MENUS           (UI_MENUS_PER_CART)
#define UI_MODEL_MAX_PAGES           (UI_PAGES_PER_MENU)
#define UI_MODEL_PARAMS_PER_PAGE     (UI_PARAMS_PER_PAGE)

/* ============================================================
 * Structures de données (modèle UI)
 * ============================================================ */

typedef struct {
    uint8_t value;
} ui_param_state_t;

typedef struct {
    ui_param_state_t params[UI_MODEL_PARAMS_PER_PAGE];
} ui_page_state_t;

typedef struct {
    ui_page_state_t pages[UI_MODEL_MAX_PAGES];
} ui_menu_state_t;

typedef struct {
    ui_menu_state_t menus[UI_MODEL_MAX_MENUS];
} ui_cart_state_t;

/**
 * @brief État complet de l’UI.
 * @ingroup ui
 */
typedef struct {
    const ui_cart_spec_t *spec;  /**< Spec UI active */
    ui_cart_state_t       vals;  /**< Valeurs RAM */
    uint8_t               cur_menu;
    uint8_t               cur_page;
    bool                  shift;
} ui_state_t;

/* ============================================================
 * API modèle
 * ============================================================ */

void ui_state_init(ui_state_t *st, const ui_cart_spec_t *spec);
void ui_model_switch_cart(const ui_cart_spec_t *spec);
void ui_model_restore_last_cart(void);
void ui_model_init(const ui_cart_spec_t* initial_spec);

const ui_cart_spec_t *ui_model_get_active_spec(void);
ui_state_t *ui_model_get_state(void);

#endif /* BRICK_UI_UI_MODEL_H */
