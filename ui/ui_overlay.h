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

/** Prépare les références de bannière (MODE/SETUP) et configure les overrides visuels.
 *  `prev_cart` doit pointer vers la cartouche "hôte" de l'overlay. Si NULL, le module
 *  tentera de réutiliser la dernière cartouche hôte connue (utile lors des cycles). */
void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               const ui_cart_spec_t** dst_mode,
                               const ui_cart_spec_t** dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag);

/** Définit l’override de bannière (nom cartouche + tag) pour l’overlay actif. */
void ui_overlay_set_banner_override(const char* cart_name, const char* tag);

/** Met à jour uniquement le tag override de la bannière active. */
void ui_overlay_update_banner_tag(const char* tag);

/** Retourne l’override courant du nom de cartouche pour l’overlay actif (ou NULL). */
const char* ui_overlay_get_banner_cart_override(void);

/** Retourne l’override courant du tag overlay pour l’overlay actif (ou NULL). */
const char* ui_overlay_get_banner_tag_override(void);

/** Cartouche hôte (celle restaurée lors de la sortie) si un overlay est actif, sinon NULL. */
const ui_cart_spec_t* ui_overlay_get_host_cart(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_OVERLAY_H */
