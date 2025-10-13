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
 *  - flag de « mode custom actif » persistant (pour rendu/règles).
 *
 * Module UI pur (pas de drivers).
 */
#ifndef BRICK_UI_UI_OVERLAY_H
#define BRICK_UI_UI_OVERLAY_H

#include <stdbool.h>
#include "ui_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_OVERLAY_NONE = 0,
    UI_OVERLAY_SEQ,
    UI_OVERLAY_ARP,
} ui_overlay_id_t;

typedef enum {
    UI_CUSTOM_NONE = 0,
    UI_CUSTOM_SEQ,
    UI_CUSTOM_ARP,
} ui_custom_mode_t;

/** Entrer dans un overlay (ferme l’éventuel overlay précédent proprement). */
void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec);

/** Quitter l’overlay courant et restaurer cart/état réels. */
void ui_overlay_exit(void);

/** Vrai si un overlay est actif. */
bool ui_overlay_is_active(void);

/** Bascule de sous-spec (ex. MODE ↔ SETUP) dans l’overlay actif. */
void ui_overlay_switch_subspec(const ui_cart_spec_t* spec);

/** Spec overlay courante (MODE/SETUP) si actif, sinon NULL. */
const ui_cart_spec_t* ui_overlay_get_spec(void);

/** Flag persistant (dernier mode custom actif). */
void ui_overlay_set_custom_mode(ui_custom_mode_t mode);
ui_custom_mode_t ui_overlay_get_custom_mode(void);

/** Prépare deux bannières (MODE/SETUP) avec cart_name + overlay_tag. */
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
