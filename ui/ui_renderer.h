/**
 * @file ui_renderer.h
 * @brief Interface de rendu graphique de l’UI Brick.
 *
 * @ingroup ui
 *
 * @details
 * Ce module gère le dessin complet de l’interface OLED :
 *  - bandeau supérieur (cartouche, tempo, note),
 *  - 4 cadres paramètre (un par encodeur),
 *  - bandeau inférieur (pages).
 *

 * Fonctions principales :
 *  - `ui_draw_frame()` : rend une frame complète à partir d’un état et d’une cart donnée.
 *  - `ui_render()`     : raccourci utilisant l’état global (fourni par le contrôleur).
 */

#ifndef BRICK_UI_UI_RENDERER_H
#define BRICK_UI_UI_RENDERER_H

#include "ui_model.h"   /* pour ui_cart_spec_t, ui_state_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Accès lecture à l’état/cart courant (fournis par le contrôleur) — forward only */
const ui_state_t*     ui_get_state(void);
const ui_cart_spec_t* ui_get_cart(void);

/**
 * @brief Rendu complet d’une frame à partir d’une cart et d’un état.
 *
 * @param cart  Pointeur vers la spécification de cartouche (`ui_cart_spec_t`).
 * @param st    Pointeur vers l’état courant de l’UI (`ui_state_t`).
 *
 * @warning Cette fonction suppose que `cart` et `st` sont valides.
 *          Aucune logique d’entrée ni de mise à jour de modèle n’est effectuée ici.
 */
void ui_draw_frame(const ui_cart_spec_t* cart, const ui_state_t* st);

/**
 * @brief Rendu simplifié : appelle `ui_draw_frame()` avec l’état global.
 *
 * Utilisé par le thread UI pour rafraîchir l’écran selon le dirty flag.
 */
void ui_render(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_RENDERER_H */
