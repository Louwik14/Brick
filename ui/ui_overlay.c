/**
 * @file ui_overlay.c
 * @brief Gestion centralisée des overlays UI (SEQ, ARP, …) pour Brick.
 * @ingroup ui
 *
 * @details
 * - Overlays exclusifs : entrer dans un overlay ferme le précédent
 *   et restaure cart/état réels avant la nouvelle entrée.
 * - Réinitialisation menu/page (0/0) à chaque entrée/switch.
 * - Publication du tag (overlay_tag) si fourni par la spec.
 * - Redraw forcé à chaque enter/exit/switch pour éviter les états "fantômes".
 */

#include <string.h>
#include "ui_overlay.h"
#include "ui_controller.h"  /* ui_switch_cart(), ui_get_cart(), ui_get_state() */
#include "ui_model.h"       /* ui_state_t, ui_model_set_active_overlay_tag() */

/* --------- État interne --------- */
static ui_overlay_id_t       s_overlay_active = UI_OVERLAY_NONE;
static const ui_cart_spec_t* s_overlay_spec   = NULL;
static const ui_cart_spec_t* s_prev_cart      = NULL;
static ui_state_t            s_prev_state;
static ui_custom_mode_t      s_custom_mode_active = UI_CUSTOM_NONE;
static const char           *s_overlay_cart_override = NULL;
static const char           *s_overlay_tag_override  = NULL;

/* --------- Helpers locaux --------- */
static inline void _publish_tag_if_any(const ui_cart_spec_t* spec) {
    const char *tag = s_overlay_tag_override;
    if ((!tag || !tag[0]) && spec && spec->overlay_tag && spec->overlay_tag[0]) {
        tag = spec->overlay_tag;
    }
    if (tag && tag[0]) {
        ui_model_set_active_overlay_tag(tag);
    }
}

static inline void _reset_overlay_indices(void) {
    ui_state_t* st = (ui_state_t*)ui_get_state();
    st->cur_menu = 0;
    st->cur_page = 0;
}

/* --------- API --------- */
void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t* spec)
{
    if (!spec) return;

    /* 1) Si déjà un overlay actif → restaurer cart & état réels et purger le contexte */
    if (s_overlay_active != UI_OVERLAY_NONE) {
        if (s_prev_cart) {
            ui_switch_cart(s_prev_cart);
            ui_state_t* st_mut = (ui_state_t*)ui_get_state();
            memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
        }
        s_prev_cart      = NULL;
        s_overlay_active = UI_OVERLAY_NONE;
        s_overlay_spec   = NULL;
    }

    /* 2) Capturer la cart & l’état RÉELS comme base de retour */
    s_prev_cart = ui_get_cart();
    memcpy(&s_prev_state, ui_get_state(), sizeof(ui_state_t));

    /* 3) Activer le NOUVEL overlay */
    s_overlay_active = id;
    s_overlay_spec   = spec;
    ui_switch_cart(spec);

    /* 4) Reset menu/page + publier tag + redraw */
    _reset_overlay_indices();
    _publish_tag_if_any(spec);
    ui_mark_dirty();
}

void ui_overlay_exit(void)
{
    if (s_overlay_active == UI_OVERLAY_NONE)
        return;

    if (s_prev_cart) {
        ui_switch_cart(s_prev_cart);
        ui_state_t* st_mut = (ui_state_t*)ui_get_state();
        memcpy(st_mut, &s_prev_state, sizeof(ui_state_t));
    }

    s_overlay_active        = UI_OVERLAY_NONE;
    s_overlay_spec          = NULL;
    s_prev_cart             = NULL;
    s_overlay_cart_override = NULL;
    s_overlay_tag_override  = NULL;

    ui_mark_dirty();
}

bool ui_overlay_is_active(void)
{
    return s_overlay_active != UI_OVERLAY_NONE;
}

void ui_overlay_switch_subspec(const ui_cart_spec_t* spec)
{
    if (!ui_overlay_is_active() || !spec)
        return;

    s_overlay_spec = spec;
    ui_switch_cart(spec);

    _reset_overlay_indices();
    _publish_tag_if_any(spec);
    ui_mark_dirty();
}

const ui_cart_spec_t* ui_overlay_get_spec(void)
{
    return s_overlay_spec;
}

void ui_overlay_set_custom_mode(ui_custom_mode_t mode)
{
    s_custom_mode_active = mode;
    /* Laisser le dernier tag affiché si besoin ; pas de reset agressif ici. */
    switch (mode) {
        case UI_CUSTOM_SEQ: ui_model_set_active_overlay_tag("SEQ"); break;
        case UI_CUSTOM_ARP: ui_model_set_active_overlay_tag("ARP"); break;
        default: break;
    }
}

ui_custom_mode_t ui_overlay_get_custom_mode(void)
{
    return s_custom_mode_active;
}

void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               const ui_cart_spec_t** dst_mode,
                               const ui_cart_spec_t** dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag)
{
    const char* banner = (prev_cart && prev_cart->cart_name && prev_cart->cart_name[0])
                             ? prev_cart->cart_name
                             : "UI";

    if (dst_mode) {
        *dst_mode = src_mode;
    }
    if (dst_setup) {
        *dst_setup = src_setup;
    }

    ui_overlay_set_banner_override(banner, mode_tag);
}

void ui_overlay_set_banner_override(const char* cart_name, const char* tag)
{
    if (cart_name && cart_name[0]) {
        s_overlay_cart_override = cart_name;
    } else {
        s_overlay_cart_override = "UI";
    }
    s_overlay_tag_override = tag;
}

void ui_overlay_update_banner_tag(const char* tag)
{
    s_overlay_tag_override = tag;
}

const char* ui_overlay_get_banner_cart_override(void)
{
    return (s_overlay_active != UI_OVERLAY_NONE) ? s_overlay_cart_override : NULL;
}

const char* ui_overlay_get_banner_tag_override(void)
{
    return (s_overlay_active != UI_OVERLAY_NONE) ? s_overlay_tag_override : NULL;
}
