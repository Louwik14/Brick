/**
 * @file ui_controller.h
 * @brief Interface principale du contrôleur UI Brick (gestion menus/pages/encodeurs).
 *
 * @ingroup ui
 *
 * Fournit la logique d’état et les callbacks de l’interface utilisateur.
 * Gère les changements de menus, pages, et valeurs d’encodeurs, et
 * propage les modifications aux cartouches via `ui_backend (pont)`.
 *
 * Architecture :
 * - `ui_controller.c` gère la logique pure (pas de rendu)
 * - `ui_renderer.c` s’occupe du dessin
 * - `ui_task.c` pilote le thread UI (poll, timing, affichage)
 */

#ifndef BRICK_UI_UI_CONTROLLER_H
#define BRICK_UI_UI_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "ui_model.h"   // inclut ui_spec.h

/* ============================================================
 * Cycle de vie
 * ============================================================ */

/**
 * @brief Initialise le contrôleur UI avec la cartouche spécifiée.
 *
 * @param spec  Pointeur vers la spécification de la cartouche active.
 */
void ui_init(const ui_cart_spec_t *spec);

/**
 * @brief Change la cartouche active et réinitialise l’état du contrôleur.
 *
 * @param spec  Nouvelle spécification de cartouche (peut être NULL).
 */
void ui_switch_cart(const ui_cart_spec_t *spec);

/* ============================================================
 * Accès (rendu, debug)
 * ============================================================ */

/** @return Pointeur vers l’état interne de l’UI (pour le renderer). */
const ui_state_t*     ui_get_state(void);

/** @return Pointeur vers la cartouche active (spécification UI). */
const ui_cart_spec_t* ui_get_cart(void);

/* ============================================================
 * GESTION DU "DIRTY FLAG" (rafraîchissement UI)
 * ============================================================
 *
 * Ces fonctions permettent au thread UI (`ui_task.c`) de savoir
 * s’il faut redessiner l’écran.
 *
 * - `ui_mark_dirty()`  → à appeler quand une action utilisateur modifie l’UI.
 * - `ui_is_dirty()`    → indique si un redraw est nécessaire.
 * - `ui_clear_dirty()` → réinitialise l’état après le rendu.
 */

/** Marque l’interface comme modifiée (force un redraw au prochain cycle). */
void ui_mark_dirty(void);

/** Indique si un rafraîchissement d’écran est nécessaire. */
bool ui_is_dirty(void);

/** Réinitialise le dirty flag après un rendu. */
void ui_clear_dirty(void);

/* ============================================================
 * MOTEUR DE CYCLE (BM)
 * ============================================================ */

/**
 * @brief Déclare les options cyclées pour un bouton-menu donné (BM1..BM8).
 *
 * @param bm_index Index du bouton (0..7).
 * @param options  Tableau de pointeurs vers des `ui_menu_spec_t` (ex: ENV Filt/Amp/Pitch).
 * @param count    Nombre d’options dans le cycle (max 4).
 */
void ui_cycles_set_options(int bm_index,
                           const ui_menu_spec_t* const* options,
                           uint8_t count);


/**
 * @brief Renvoie le menu actif pour ce bouton.
 *
 * @param bm_index Index du bouton (0..7).
 * @return Pointeur vers le `ui_menu_spec_t` actif (cartouche ou cycle).
 */
const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index);

/* ============================================================
 * OPTIONS DE COMPORTEMENT
 * ============================================================ */

/**
 * @brief Active/désactive le mode de reprise de cycle.
 *
 * @param enable
 * - `false` = revenir toujours au premier menu du cycle.
 * - `true`  = reprendre le dernier menu actif.
 */
void ui_set_cycle_resume_mode(bool enable);

/**
 * @brief Indique si le mode “resume cycle” est actif.
 *
 * @return `true` si la reprise automatique est activée.
 */
bool ui_get_cycle_resume_mode(void);

/* ============================================================
 * Handlers d’événements (appelés par ui_task)
 * ============================================================ */

/**
 * @brief Gère l’appui sur un bouton menu (BM1..BM8).
 *
 * @param index Index du bouton (0..7).
 */
void ui_on_button_menu(int index);

/**
 * @brief Gère l’appui sur un bouton de page (P1..P5).
 *
 * @param index Index du bouton (0..4).
 */
void ui_on_button_page(int index);

/**
 * @brief Gère la rotation d’un encodeur.
 *
 * @param enc_index Index de l’encodeur (0..3).
 * @param delta     Variation de rotation (signée).
 *
 * @note Cette fonction appelle `cart_link_param_changed()` avec
 *       `(dest_id, value, is_bitwise, bit_mask)`.
 */
void ui_on_encoder(int enc_index, int delta);



/**
 * @brief Hook de configuration des cycles pour une cart donnée.
 *
 * @details
 * Weak par défaut (implémentation vide). Chaque cart peut fournir
 * sa propre implémentation forte dans son fichier `cart_*.c` pour
 * déclarer les groupes cyclés BM via `ui_cycles_set_options()`.
 *
 * @param spec  Cart UI courante.
 * @ingroup ui_controller
 */
void ui_cycles_setup_for(const ui_cart_spec_t* spec);


#endif /* BRICK_UI_UI_CONTROLLER_H */
