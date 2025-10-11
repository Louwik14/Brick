/**
 * @file ui_controller.h
 * @brief Logique centrale de contrôle de l’interface utilisateur Brick.
 * @ingroup ui
 *
 * @details
 * Ce module traduit les **entrées physiques** (boutons, encodeurs) en
 * modifications d’état de l’UI, selon la **spécification UI** de la cartouche
 * active (`ui_cart_spec_t`).
 *
 * Principales responsabilités :
 * - Maintenir l’état courant (`ui_state_t`) et ses valeurs (`ui_model`).
 * - Gérer les **cycles BM** (menus rotatifs) déclarés dans la spec.
 * - Propager les changements de paramètres via `ui_backend` (pont neutre).
 * - Signaler les changements de rendu via un **dirty flag**.
 *
 * Invariants :
 * - Aucun accès direct au bus, UART ou drivers matériels.
 * - Le contrôleur ne fait que **modifier l’état UI** et appeler `ui_backend`.
 * - Le renderer est purement stateless (lit uniquement l’état courant).
 */

#ifndef BRICK_UI_UI_CONTROLLER_H
#define BRICK_UI_UI_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_model.h"   /* ui_state_t */
#include "ui_spec.h"    /* ui_cart_spec_t */

/* ============================================================
 * API publique
 * ============================================================ */

/**
 * @brief Initialise l’état UI avec la spécification donnée.
 * @param spec Spécification de cartouche active.
 * @ingroup ui
 */
void ui_init(const ui_cart_spec_t *spec);

/**
 * @brief Bascule sur une nouvelle cartouche UI.
 * @param spec Nouvelle spécification UI active.
 * @ingroup ui
 */
void ui_switch_cart(const ui_cart_spec_t *spec);

/**
 * @brief Retourne l’état courant de l’UI.
 * @return Pointeur constant sur l’état global (`ui_state_t`).
 * @ingroup ui
 */
const ui_state_t* ui_get_state(void);

/**
 * @brief Retourne la spécification UI active.
 * @return Pointeur constant sur `ui_cart_spec_t`.
 * @ingroup ui
 */
const ui_cart_spec_t* ui_get_cart(void);

/**
 * @brief Résout le menu actif pour le rendu.
 * @param bm_index Index du bouton menu (ignoré, pour compat).
 * @return Pointeur vers la spec du menu actif.
 * @ingroup ui
 */
const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index);

/* ============================================================
 * Entrées utilisateur
 * ============================================================ */

/**
 * @brief Gestion d’un appui sur un bouton **menu** (BM1..BM8).
 * @param index Index du bouton.
 * @ingroup ui
 */
void ui_on_button_menu(int index);

/**
 * @brief Gestion d’un appui sur un bouton **page** (P1..P5).
 * @param index Index du bouton page.
 * @ingroup ui
 */
void ui_on_button_page(int index);

/**
 * @brief Gestion du mouvement d’un **encodeur** (0..3) sur la page courante.
 * @param enc_index Index de l’encodeur (0..3).
 * @param delta Incrément ou décrément (±1 typiquement).
 * @ingroup ui
 */
void ui_on_encoder(int enc_index, int delta);

/* ============================================================
 * Dirty flag (rafraîchissement rendu)
 * ============================================================ */

/**
 * @brief Marque l’état UI comme « dirty » (à redessiner).
 * @ingroup ui
 */
void ui_mark_dirty(void);

/**
 * @brief Indique si l’état UI est « dirty » (doit être rerendu).
 * @return true si l’état a changé.
 * @ingroup ui
 */
bool ui_is_dirty(void);

/**
 * @brief Efface le flag dirty après un rendu.
 * @ingroup ui
 */
void ui_clear_dirty(void);

#endif /* BRICK_UI_UI_CONTROLLER_H */
