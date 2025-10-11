/**
 * @file ui_model.h
 * @brief État mutable de l’interface utilisateur Brick (RAM UI).
 * @ingroup ui
 *
 * @details
 * Ce module maintient l’état logique courant de l’UI (valeurs de paramètres,
 * menu/page actifs, tag persistant d’overlay, etc.). Les structures sont
 * alignées sur la spécification `ui_spec.h`, qui reste la source de vérité.
 *
 * Les valeurs des paramètres sont stockées dans `ui_param_state_t.value` :
 * - pour les paramètres **unipolaires**, la valeur est positive (0..max).
 * - pour les paramètres **bipolaires**, la valeur peut être **signée**
 *   (ex. -12..+12, -128..+127, etc.).
 *
 * La couche `ui_controller` traduit ensuite ces valeurs en représentation
 * "wire" (uint8_t) avant routage vers la cartouche via `ui_backend`.
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

/**
 * @struct ui_param_state_t
 * @brief État courant d’un paramètre UI.
 * @ingroup ui
 *
 * @details
 * La valeur est conservée dans le **domaine UI réel** :
 * - `int16_t` permet de stocker des plages négatives ou supérieures à 255.
 * - Les conversions vers `uint8_t` (wire) sont réalisées par le contrôleur.
 */
typedef struct {
    int16_t value;  /**< Valeur UI réelle, signée si besoin (plage définie par ui_spec). */
} ui_param_state_t;

/**
 * @struct ui_page_state_t
 * @brief État d’une page (ensemble de paramètres).
 * @ingroup ui
 */
typedef struct {
    ui_param_state_t params[UI_MODEL_PARAMS_PER_PAGE];
} ui_page_state_t;

/**
 * @struct ui_menu_state_t
 * @brief État d’un menu UI (ensemble de pages).
 * @ingroup ui
 */
typedef struct {
    ui_page_state_t pages[UI_MODEL_MAX_PAGES];
} ui_menu_state_t;

/**
 * @struct ui_cart_state_t
 * @brief État complet d’une cartouche UI (ensemble de menus).
 * @ingroup ui
 */
typedef struct {
    ui_menu_state_t menus[UI_MODEL_MAX_MENUS];
} ui_cart_state_t;

/**
 * @struct ui_state_t
 * @brief État global de l’UI.
 * @ingroup ui
 *
 * @details
 * Contient :
 * - un pointeur vers la spec UI active (`ui_cart_spec_t`);
 * - la matrice de valeurs (`vals`);
 * - le menu et la page actifs ;
 * - l’état du bouton SHIFT.
 */
typedef struct {
    const ui_cart_spec_t *spec;  /**< Spécification UI active. */
    ui_cart_state_t       vals;  /**< Valeurs courantes en RAM UI. */
    uint8_t               cur_menu; /**< Menu actif. */
    uint8_t               cur_page; /**< Page active. */
    bool                  shift;    /**< État du modifieur SHIFT. */
} ui_state_t;

/* ============================================================
 * API modèle
 * ============================================================ */

/**
 * @brief Initialise un état UI à partir d’une spec.
 * @param st   Pointeur vers la structure d’état.
 * @param spec Spécification UI de la cartouche active.
 * @ingroup ui
 */
void ui_state_init(ui_state_t *st, const ui_cart_spec_t *spec);

/**
 * @brief Bascule sur une nouvelle cartouche UI (réinitialise l’état).
 * @param spec Nouvelle spécification UI active.
 * @ingroup ui
 */
void ui_model_switch_cart(const ui_cart_spec_t *spec);

/**
 * @brief Restaure la dernière cartouche UI active (pile 1 niveau).
 * @ingroup ui
 */
void ui_model_restore_last_cart(void);

/**
 * @brief Initialise le modèle global (état + cartouche initiale).
 * @param initial_spec Spécification UI de départ.
 * @ingroup ui
 */
void ui_model_init(const ui_cart_spec_t* initial_spec);

/**
 * @brief Récupère le tag du mode custom actif (SEQ, ARP…).
 * @return Chaîne persistante, `"SEQ"` par défaut si non défini.
 * @ingroup ui
 */
const char *ui_model_get_active_overlay_tag(void);

/**
 * @brief Définit le tag texte du mode custom actif.
 * @param tag Chaîne C (ex: "SEQ", "ARP"). NULL ou "" pour effacer.
 * @ingroup ui
 */
void ui_model_set_active_overlay_tag(const char *tag);

/**
 * @brief Retourne la spec UI actuellement active.
 * @ingroup ui
 */
const ui_cart_spec_t *ui_model_get_active_spec(void);

/**
 * @brief Retourne un pointeur vers l’état UI global (mutable).
 * @ingroup ui
 */
ui_state_t *ui_model_get_state(void);

#endif /* BRICK_UI_UI_MODEL_H */
