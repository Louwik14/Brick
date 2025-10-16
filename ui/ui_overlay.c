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
typedef struct {
    bool                    active;
    ui_overlay_id_t         id;
    const ui_cart_spec_t   *spec;
    const ui_cart_spec_t   *host_cart;
    ui_state_t              host_state;
    ui_custom_mode_t        custom_mode;
    const char             *banner_cart_override;
    const char             *banner_tag_override;
} ui_overlay_session_t;

static ui_overlay_session_t s_session = {
    .active                 = false,
    .id                     = UI_OVERLAY_NONE,
    .spec                   = NULL,
    .host_cart              = NULL,
    .host_state             = { 0 },
    .custom_mode            = UI_CUSTOM_NONE,
    .banner_cart_override   = NULL,
    .banner_tag_override    = NULL,
};

/* --------- Helpers locaux --------- */
static inline void _publish_tag_if_any(const ui_cart_spec_t* spec) {
    const char *tag = s_session.banner_tag_override;
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
    if (s_session.active) {
        if (s_session.host_cart) {
            ui_switch_cart(s_session.host_cart);
            ui_state_t* st_mut = (ui_state_t*)ui_get_state();
            memcpy(st_mut, &s_session.host_state, sizeof(ui_state_t));
        }
        s_session.active               = false;
        s_session.host_cart            = NULL;
        s_session.spec                 = NULL;
        s_session.id                   = UI_OVERLAY_NONE;
    }

    /* 2) Capturer la cart & l’état RÉELS comme base de retour */
    s_session.host_cart = ui_get_cart();
    memcpy(&s_session.host_state, ui_get_state(), sizeof(ui_state_t));

    /* 3) Activer le NOUVEL overlay */
    s_session.id   = id;
    s_session.spec = spec;
    s_session.active = true;
    ui_switch_cart(spec);

    /* 4) Reset menu/page + publier tag + redraw */
    _reset_overlay_indices();
    _publish_tag_if_any(spec);
    ui_mark_dirty();
}

void ui_overlay_exit(void)
{
    if (!s_session.active)
        return;

    if (s_session.host_cart) {
        ui_switch_cart(s_session.host_cart);
        ui_state_t* st_mut = (ui_state_t*)ui_get_state();
        memcpy(st_mut, &s_session.host_state, sizeof(ui_state_t));
    }

    s_session.active               = false;
    s_session.spec                 = NULL;
    s_session.host_cart            = NULL;
    s_session.id                   = UI_OVERLAY_NONE;
    s_session.banner_cart_override = NULL;
    s_session.banner_tag_override  = NULL;

    ui_mark_dirty();
}

bool ui_overlay_is_active(void)
{
    return s_session.active;
}

void ui_overlay_switch_subspec(const ui_cart_spec_t* spec)
{
    if (!ui_overlay_is_active() || !spec)
        return;

    s_session.spec = spec;
    ui_switch_cart(spec);

    _reset_overlay_indices();
    _publish_tag_if_any(spec);
    ui_mark_dirty();
}

const ui_cart_spec_t* ui_overlay_get_spec(void)
{
    return s_session.spec;
}

void ui_overlay_set_custom_mode(ui_custom_mode_t mode)
{
    s_session.custom_mode = mode;
    /* Laisser le dernier tag affiché si besoin ; pas de reset agressif ici. */
    switch (mode) {
        case UI_CUSTOM_SEQ: ui_model_set_active_overlay_tag("SEQ"); break;
        case UI_CUSTOM_ARP: ui_model_set_active_overlay_tag("ARP"); break;
        default: break;
    }
}

ui_custom_mode_t ui_overlay_get_custom_mode(void)
{
    return s_session.custom_mode;
}

void ui_overlay_prepare_banner(const ui_cart_spec_t* src_mode,
                               const ui_cart_spec_t* src_setup,
                               const ui_cart_spec_t** dst_mode,
                               const ui_cart_spec_t** dst_setup,
                               const ui_cart_spec_t* prev_cart,
                               const char* mode_tag)
{
    const ui_cart_spec_t* banner_cart = prev_cart;
    if (!banner_cart && s_session.active) {
        banner_cart = s_session.host_cart;
    }
    if (banner_cart == src_mode || banner_cart == src_setup) {
        banner_cart = s_session.host_cart;
    }

    const char* banner = (banner_cart && banner_cart->cart_name && banner_cart->cart_name[0])
                             ? banner_cart->cart_name
                             : (s_session.host_cart && s_session.host_cart->cart_name
                                    ? s_session.host_cart->cart_name
                                    : "UI");

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
        s_session.banner_cart_override = cart_name;
    } else {
        if (s_session.host_cart && s_session.host_cart->cart_name && s_session.host_cart->cart_name[0]) {
            s_session.banner_cart_override = s_session.host_cart->cart_name;
        } else {
            s_session.banner_cart_override = "UI";
        }
    }
    s_session.banner_tag_override = tag;
}

void ui_overlay_update_banner_tag(const char* tag)
{
    s_session.banner_tag_override = tag;
}

const char* ui_overlay_get_banner_cart_override(void)
{
    return s_session.active ? s_session.banner_cart_override : NULL;
}

const char* ui_overlay_get_banner_tag_override(void)
{
    return s_session.active ? s_session.banner_tag_override : NULL;
}

const ui_cart_spec_t* ui_overlay_get_host_cart(void)
{
    return s_session.active ? s_session.host_cart : NULL;
}
