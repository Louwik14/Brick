/**
 * @file ui_overlay.c
 * @brief Implémentation de la gestion des overlays UI (SEQ, ARP, …).
 * @ingroup ui
 *
 * @details
 * Module UI pur : entrée/sortie d’overlay, bascule de sous-spec et
 * flag « mode custom actif » persistant (pour affichage/règles de pas).
 */

#include <string.h>
#include "ui_overlay.h"
#include "ui_controller.h"  /* ui_switch_cart(), ui_get_cart(), ui_get_state() */
#include "ui_model.h"       /* ui_state_t */

/* ============================================================
 * États internes
 * ============================================================ */

/** Overlay actif (exclusif) ; NONE si aucun. */
static ui_overlay_id_t s_overlay_active = UI_OVERLAY_NONE;

/** Spec overlay courante (MODE ou SETUP) si actif. */
static const ui_cart_spec_t* s_overlay_spec = NULL;

/** Cart “réelle” sauvegardée à l’entrée du 1er overlay. */
static const ui_cart_spec_t* s_prev_cart = NULL;

/** État complet “réel” sauvegardé à l’entrée du 1er overlay. */
static ui_state_t s_prev_state;

/** Mode custom actif persistant (affichage/règles steps). */
static ui_custom_mode_t s_custom_mode_active = UI_CUSTOM_NONE;

/* ============================================================
 * API
 * ============================================================ */

void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec) {
    const ui_cart_spec_t* cur = ui_get_cart();

    /* Si un overlay était déjà actif → restaurer d’abord la cart/état réels */
    if (s_overlay_active != UI_OVERLAY_NONE) {
        if (s_prev_cart) {
            ui_switch_cart(s_prev_cart);
            ui_state_t* st_mut = (ui_state_t*)ui_get_state();
            memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
        }
        s_prev_cart = NULL;
        s_overlay_active = UI_OVERLAY_NONE;
        s_overlay_spec = NULL;
    }

    /* Première entrée : sauvegarder cart & état réels */
    s_prev_cart = cur;
    memcpy(&s_prev_state, ui_get_state(), sizeof(ui_state_t));

    /* Activer l’overlay demandé */
    s_overlay_active = id;
    s_overlay_spec   = spec;
    ui_switch_cart(spec);

    /* Réinitialiser menu/page (0/0) pour lisibilité */
    ui_state_t* st = (ui_state_t*)ui_get_state();
    st->cur_menu = 0;
    st->cur_page = 0;
}

void ui_overlay_exit(void) {
    if (s_overlay_active == UI_OVERLAY_NONE) return;

    if (s_prev_cart) {
        ui_switch_cart(s_prev_cart);
        ui_state_t* st_mut = (ui_state_t*)ui_get_state();
        memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
    }

    s_overlay_active = UI_OVERLAY_NONE;
    s_overlay_spec   = NULL;
    s_prev_cart      = NULL;
}

bool ui_overlay_is_active(void) {
    return s_overlay_active != UI_OVERLAY_NONE;
}

void ui_overlay_switch_subspec(const ui_cart_spec_t* spec) {
    if (!ui_overlay_is_active()) return;

    s_overlay_spec = spec;
    ui_switch_cart(spec);

    /* Reset en-tête pour consistance */
    ui_state_t* st = (ui_state_t*)ui_get_state();
    st->cur_menu = 0;
    st->cur_page = 0;
}

const ui_cart_spec_t* ui_overlay_get_spec(void) {
    return s_overlay_spec;
}

/* --------- Mode custom persistant --------- */

void ui_overlay_set_custom_mode(ui_custom_mode_t mode) {
    s_custom_mode_active = mode;
    /* Publie aussi le tag texte persistant dans le modèle pour le renderer */
    switch (mode) {
        case UI_CUSTOM_SEQ: ui_model_set_active_overlay_tag("SEQ"); break;
        case UI_CUSTOM_ARP: ui_model_set_active_overlay_tag("ARP"); break;
        case UI_CUSTOM_NONE:
        default:
            /* On peut choisir de conserver le dernier tag (comportement demandé) */
            /* ui_model_set_active_overlay_tag(""); // si tu voulais l’effacer */
            break;
    }
}

ui_custom_mode_t ui_overlay_get_custom_mode(void) {
    return s_custom_mode_active;
}

/* --------- Préparation bannières (MODE/SETUP) --------- */

void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               ui_cart_spec_t* dst_mode,
                               ui_cart_spec_t* dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag)
{
    const char* banner = (prev_cart && prev_cart->cart_name) ? prev_cart->cart_name : "UI";

    /* Copie superficielle : les sous-structs restent partagées (pages/params). */
    *dst_mode  = *src_mode;
    *dst_setup = *src_setup;

    /* Insertion du nom & tag overlay (pour futur affichage) */
    dst_mode->cart_name  = banner;
    dst_setup->cart_name = banner;

    dst_mode->overlay_tag  = mode_tag;
    dst_setup->overlay_tag = mode_tag;
}
