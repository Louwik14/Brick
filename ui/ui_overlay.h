/**
 * @file ui_overlay.h
 * @brief Gestion centralisée des overlays UI (SEQ, ARP, …).
 * @ingroup ui
 *
 * @details
 * Encapsule :
 *  - entrée/sortie d’un overlay,
 *  - bascule de sous-spec (MODE↔SETUP),
 *  - accès à la spec overlay courante,
 *  - flag de « mode custom actif » persistant (pour rendu/steps).
 *
 * Conforme README: module UI pur (aucun accès bus/driver).
 */
#ifndef BRICK_UI_UI_OVERLAY_H
#define BRICK_UI_UI_OVERLAY_H

#include <stdbool.h>
#include "ui_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Types d’overlays supportés. */
typedef enum {
    UI_OVERLAY_NONE = 0,
    UI_OVERLAY_SEQ,
    UI_OVERLAY_ARP,
    /* futurs: UI_OVERLAY_FX, UI_OVERLAY_DRUM, ... */
} ui_overlay_id_t;

/** Types de modes custom (flag persistant visuel/logic). */
typedef enum {
    UI_CUSTOM_NONE = 0,
    UI_CUSTOM_SEQ,
    UI_CUSTOM_ARP,
} ui_custom_mode_t;

/* ===== API principale ===== */

/**
 * @brief Entrer dans un overlay (remplace un overlay précédent si présent).
 * Sauvegarde la cartouche/état réels à la première entrée, puis bascule sur @p spec.
 */
void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec);

/**
 * @brief Quitter l’overlay courant et restaurer la cart/état réels.
 * Ne réinitialise PAS le flag de mode custom (persistance voulue).
 */
void ui_overlay_exit(void);

/** @brief Vrai si un overlay est actif. */
bool ui_overlay_is_active(void);

/**
 * @brief Bascule vers une autre sous-spec de l’overlay courant (ex. MODE↔SETUP).
 * Ne modifie pas la cart/état réels sauvegardés.
 */
void ui_overlay_switch_subspec(const ui_cart_spec_t* spec);

/** @brief Retourne la spec overlay actuellement affichée, ou NULL si aucun. */
const ui_cart_spec_t* ui_overlay_get_spec(void);

/* ===== Flag de mode custom persistant ===== */

/** @brief Définit le mode custom actif (persiste même après exit overlay). */
void ui_overlay_set_custom_mode(ui_custom_mode_t mode);

/** @brief Lit le mode custom actif persistant. */
ui_custom_mode_t ui_overlay_get_custom_mode(void);

/* ===== Utilitaire : préparer des « bannières » overlay avec tag ===== */
/**
 * @brief Prépare deux copies de specs (MODE/SETUP) pour un overlay,
 *        en injectant le nom de la cartouche réelle et un tag (ex: "SEQ").
 *
 * @param src_mode   Spec source MODE (ex: seq_ui_spec)
 * @param src_setup  Spec source SETUP (ex: seq_setup_ui_spec)
 * @param dst_mode   Copie destination MODE (écrasée)
 * @param dst_setup  Copie destination SETUP (écrasée)
 * @param prev_cart  Cartouche réelle à afficher en en-tête
 * @param mode_tag   Tag texte (ex: "SEQ", "ARP") stocké pour rendu futur
 */
void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               ui_cart_spec_t* dst_mode,
                               ui_cart_spec_t* dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_OVERLAY_H */
