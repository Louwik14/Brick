/**
 * @file ui_shortcuts.c
 * @brief Raccourcis clavier (SHIFT + …), machine MUTE/PMUTE et intégration LED contextuelle.
 * @ingroup ui
 *
 * @details
 * Priorité de gestion :
 *   1) MUTE (QUICK / PMUTE)
 *   2) Overlays (SEQ/ARP/KEYBOARD) — désactivés quand MUTE actif
 *   3) Transport & autres
 *
 * Points clés :
 * - Le flag runtime `s_keys_active` représente l’activation du mode Keys (Keyboard)
 *   même si l’overlay n’est pas visible (utile pour que +/- fonctionne dans le menu paramètres).
 * - À l’entrée de Keys (SHIFT+SEQ11) : si Keys est déjà affiché, on sort puis on rouvre
 *   proprement la bannière afin d’éviter la disparition du cart_name.
 * - En sortie de MUTE : on restaure LEDs + tag en fonction du contexte (Keys/SEQ/ARP/aucun).
 */

#include <string.h>
#include <stdio.h>

#include "ui_shortcuts.h"

#include "ui_input.h"         /* ui_input_event_t */
#include "ui_controller.h"
#include "ui_overlay.h"
#include "ui_model.h"
#include "ui_mute_backend.h"
#include "ui_led_backend.h"
#include "cart_registry.h"
#include "drv_buttons_map.h"

#include "ui_seq_ui.h"
#include "ui_arp_ui.h"
#include "ui_keyboard_ui.h"   /* vitrine Keyboard */
#include "ui_keyboard_app.h"  /* pour lire l’octave shift courant */

/* ======================================================================
 * Bannières overlay (copies peu profondes des specs réelles)
 * ====================================================================== */
static ui_cart_spec_t s_seq_mode_spec_banner;
static ui_cart_spec_t s_seq_setup_spec_banner;

static ui_cart_spec_t s_arp_mode_spec_banner;
static ui_cart_spec_t s_arp_setup_spec_banner;

/* Banner clone pour KEYBOARD (hérite du nom de la cart active) */
static ui_cart_spec_t s_kbd_spec_banner;

/* ======================================================================
 * Machine MUTE & état Keys
 * ====================================================================== */
typedef enum {
    MUTE_OFF = 0,
    MUTE_QUICK,
    MUTE_PMUTE
} mute_state_t;

static mute_state_t s_mute      = MUTE_OFF;
static bool         s_plus_down = false;
static bool         s_last_shift= false;

/* Flag runtime : Keys (Keyboard) actif même si overlay invisible */
static bool         s_keys_active = false;

/* ======================================================================
 * Helpers locaux
 * ====================================================================== */
static inline bool _is_bm(uint8_t btn) {
    return (btn == UI_BTN_PARAM1) || (btn == UI_BTN_PARAM2) ||
           (btn == UI_BTN_PARAM3) || (btn == UI_BTN_PARAM4) ||
           (btn == UI_BTN_PARAM5) || (btn == UI_BTN_PARAM6) ||
           (btn == UI_BTN_PARAM7) || (btn == UI_BTN_PARAM8);
}

static inline bool _is_seq1_16(uint8_t btn) {
    return (btn >= UI_BTN_SEQ1) && (btn <= UI_BTN_SEQ16);
}

static inline uint8_t _seq_btn_to_track(uint8_t btn) {
    return (uint8_t)(btn - UI_BTN_SEQ1);
}

/* Overlay courant == Keys ? (détection par nom du 1er menu) */
static bool _overlay_is_keys_current(void) {
    if (!ui_overlay_is_active()) return false;
    const ui_cart_spec_t* cur = ui_overlay_get_spec();
    if (!cur) return false;
    const char* name = cur->menus[0].name;
    return (name && strcmp(name, "KEYBOARD") == 0);
}

/* Construit et publie le tag "Keys" ou "Keys ±N" selon l’octave shift courant */
static void _publish_keys_tag_from_current_shift(void) {
    const int8_t shift = ui_keyboard_app_get_octave_shift();
    char tag[32];
    if (shift == 0) {
        snprintf(tag, sizeof(tag), "KEY");
    } else {
        snprintf(tag, sizeof(tag), "KEY%+d", (int)shift);
    }
    ui_model_set_active_overlay_tag(tag);
}

/* Neutralise overlay_tag des bannières pendant MUTE (pour imposer MUTE/PMUTE) */
static void _neutralize_overlay_tag_for_mute_if_overlay_active(void) {
    if (!ui_overlay_is_active()) return;
    const ui_cart_spec_t* cur = ui_overlay_get_spec();
    if (!cur) return;

    if (cur == &s_seq_mode_spec_banner || cur == &s_seq_setup_spec_banner) {
        s_seq_mode_spec_banner.overlay_tag   = NULL;
        s_seq_setup_spec_banner.overlay_tag  = NULL;
    } else if (cur == &s_arp_mode_spec_banner || cur == &s_arp_setup_spec_banner) {
        s_arp_mode_spec_banner.overlay_tag   = NULL;
        s_arp_setup_spec_banner.overlay_tag  = NULL;
    } else if (cur == &s_kbd_spec_banner) {
        s_kbd_spec_banner.overlay_tag = NULL;
    }
}

static void _restore_overlay_banner_tags(void) {
    s_seq_mode_spec_banner.overlay_tag   = "SEQ";
    s_seq_setup_spec_banner.overlay_tag  = "SEQ";
    s_arp_mode_spec_banner.overlay_tag   = "ARP";
    s_arp_setup_spec_banner.overlay_tag  = "ARP";
    /* KEYBOARD: on laisse NULL pour permettre le tag dynamique modèle */
    s_kbd_spec_banner.overlay_tag        = NULL;
}

/* Restaure le tag modèle et le mode LED adaptés au contexte courant */
static void _restore_overlay_visuals_after_mute(void) {
    const ui_cart_spec_t* cur = ui_overlay_get_spec(); /* peut être NULL si aucun overlay */
    if (cur == &s_seq_mode_spec_banner || cur == &s_seq_setup_spec_banner) {
        ui_model_set_active_overlay_tag("SEQ");
        ui_led_backend_set_mode(UI_LED_MODE_SEQ);
    } else if (cur == &s_arp_mode_spec_banner || cur == &s_arp_setup_spec_banner) {
        ui_model_set_active_overlay_tag("ARP");
        ui_led_backend_set_mode(UI_LED_MODE_ARP);
    } else if (cur == &s_kbd_spec_banner) {
        /* Overlay Keys actif */
        _publish_keys_tag_from_current_shift();
        ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
        s_keys_active = true;
    } else {
        /* Aucun overlay affiché : si Keys était actif avant MUTE, on restaure juste le mode LED */
        if (s_keys_active) {
            _publish_keys_tag_from_current_shift();
            ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
        } else {
            ui_model_set_active_overlay_tag("");
            ui_led_backend_set_mode(UI_LED_MODE_NONE);
        }
    }
}

/* ======================================================================
 * 1) MUTE — priorité maximale
 * ====================================================================== */
static bool handle_mute(const ui_input_event_t *evt) {
    const bool shift_now = ui_input_shift_is_pressed();

    if (evt->has_button && evt->btn_id == UI_BTN_PLUS) {
        s_plus_down = evt->btn_pressed;
    }

    /* Entrée QUICK : SHIFT + PLUS (press) */
    if (evt->has_button && evt->btn_pressed &&
        evt->btn_id == UI_BTN_PLUS && shift_now && s_mute == MUTE_OFF) {

        s_mute = MUTE_QUICK;
        s_last_shift = shift_now;

        ui_led_backend_set_mode(UI_LED_MODE_MUTE);
        ui_model_set_active_overlay_tag("MUTE");
        _neutralize_overlay_tag_for_mute_if_overlay_active();
        ui_mark_dirty();
        return true;
    }

    /* Transition QUICK → PMUTE (maintien +, relâche/represse SHIFT) */
    if (s_mute == MUTE_QUICK && s_plus_down) {
        if (!shift_now && s_last_shift) {
            s_last_shift = false; /* front descendant */
        } else if (shift_now && !s_last_shift) {
            s_mute = MUTE_PMUTE;
            ui_led_backend_set_mode(UI_LED_MODE_MUTE);
            ui_model_set_active_overlay_tag("PMUTE");
            _neutralize_overlay_tag_for_mute_if_overlay_active();
            ui_mark_dirty();
            s_last_shift = true;
        }
    }

    /* QUICK : toggle SEQ1.16 */
    if (s_mute == MUTE_QUICK && evt->has_button && _is_seq1_16(evt->btn_id)) {
        if (evt->btn_pressed) {
            const uint8_t track = _seq_btn_to_track(evt->btn_id);
            if (track < 16) ui_mute_backend_toggle(track);
        }
        return true;
    }

    /* Sortie QUICK : relâche du PLUS */
    if (s_mute == MUTE_QUICK && evt->has_button &&
        evt->btn_id == UI_BTN_PLUS && !evt->btn_pressed) {

        s_mute = MUTE_OFF;
        _restore_overlay_banner_tags();
        _restore_overlay_visuals_after_mute();
        ui_mark_dirty();
        return true;
    }

    /* PMUTE : préparation SEQ1.16 */
    if (s_mute == MUTE_PMUTE && evt->has_button && _is_seq1_16(evt->btn_id)) {
        if (evt->btn_pressed) {
            const uint8_t track = _seq_btn_to_track(evt->btn_id);
            if (track < 16) ui_mute_backend_toggle_prepare(track);
        }
        return true;
    }

    /* PMUTE : commit via PLUS (press, sans SHIFT) */
    if (s_mute == MUTE_PMUTE && evt->has_button &&
        evt->btn_id == UI_BTN_PLUS && evt->btn_pressed && !shift_now) {

        ui_mute_backend_commit();
        s_mute = MUTE_OFF;
        s_plus_down = true;
        _restore_overlay_banner_tags();
        _restore_overlay_visuals_after_mute();
        ui_mark_dirty();
        return true;
    }

    return false;
}

/* ======================================================================
 * 2) OVERLAYS — désactivés quand MUTE actif
 * ====================================================================== */
static bool handle_overlays(const ui_input_event_t *evt) {
    if (s_mute != MUTE_OFF) return false; /* garde-fou : pas d’overlay pendant MUTE */

    if (!evt->has_button || !evt->btn_pressed) return false;

    /* Quitter un overlay actif avec un bouton menu (BM1.BM8) */
    if (ui_overlay_is_active() && _is_bm(evt->btn_id)) {
        ui_overlay_exit();
        /* On ne change pas s_keys_active ici : la logique “mode actif” ne dépend pas de l’overlay visible */
        return false;
    }

    /* SHIFT + SEQ9 → Overlay SEQ (MODE/SETUP) */
    if (ui_input_shift_is_pressed() && evt->btn_id == UI_BTN_SEQ9) {
        ui_overlay_prepare_banner(&seq_ui_spec, &seq_setup_ui_spec,
                                  &s_seq_mode_spec_banner, &s_seq_setup_spec_banner,
                                  ui_get_cart(), "SEQ");
        ui_overlay_set_custom_mode(UI_CUSTOM_SEQ);
        s_keys_active = false; /* on bascule le contexte actif */

        if (!ui_overlay_is_active()) {
            ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
        } else if (ui_overlay_get_spec() == &s_seq_mode_spec_banner) {
            ui_overlay_switch_subspec(&s_seq_setup_spec_banner);
        } else if (ui_overlay_get_spec() == &s_seq_setup_spec_banner) {
            ui_overlay_switch_subspec(&s_seq_mode_spec_banner);
        } else {
            ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
        }
        ui_led_backend_set_mode(UI_LED_MODE_SEQ);
        ui_model_set_active_overlay_tag("SEQ");
        ui_mark_dirty();
        return true;
    }

    /* SHIFT + SEQ10 → Overlay ARP (MODE/SETUP) */
    if (ui_input_shift_is_pressed() && evt->btn_id == UI_BTN_SEQ10) {
        ui_overlay_prepare_banner(&arp_ui_spec, &arp_setup_ui_spec,
                                  &s_arp_mode_spec_banner, &s_arp_setup_spec_banner,
                                  ui_get_cart(), "ARP");
        ui_overlay_set_custom_mode(UI_CUSTOM_ARP);
        s_keys_active = false;

        if (!ui_overlay_is_active()) {
            ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
        } else if (ui_overlay_get_spec() == &s_arp_mode_spec_banner) {
            ui_overlay_switch_subspec(&s_arp_setup_spec_banner);
        } else if (ui_overlay_get_spec() == &s_arp_setup_spec_banner) {
            ui_overlay_switch_subspec(&s_arp_mode_spec_banner);
        } else {
            ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
        }
        ui_led_backend_set_mode(UI_LED_MODE_ARP);
        ui_model_set_active_overlay_tag("ARP");
        ui_mark_dirty();
        return true;
    }

    /* SHIFT + SEQ11 → Overlay KEYBOARD (menu unique + banner clone) */
    if (ui_input_shift_is_pressed() && evt->btn_id == UI_BTN_SEQ11) {

        /* Si Keys est déjà l’overlay courant, sortir puis r-entrer proprement */
        if (_overlay_is_keys_current()) {
            ui_overlay_exit();
        }

        /* Clone léger de la spec Keyboard pour hériter du nom de la cart active */
        s_kbd_spec_banner = ui_keyboard_spec; /* copie structurelle */

        /* Toujours recalculer le nom de cartouche pour éviter la disparition */
        const ui_cart_spec_t* cart_spec = ui_get_cart();
        s_kbd_spec_banner.cart_name   = cart_spec ? cart_spec->cart_name : "";

        /* IMPORTANT: overlay_tag = NULL → le renderer prendra le tag du modèle (dynamique) */
        s_kbd_spec_banner.overlay_tag = NULL;

        ui_overlay_set_custom_mode(UI_CUSTOM_NONE); /* tag géré côté modèle */
        ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
        s_keys_active = true;

        /* Publier le tag selon l’octave shift courant */
        _publish_keys_tag_from_current_shift();

        ui_overlay_enter(UI_OVERLAY_SEQ, &s_kbd_spec_banner);
        ui_mark_dirty();
        return true;
    }

    return false;
}

/* ======================================================================
 * 3) TRANSPORT & AUTRES (réservés / inchangés)
 * ====================================================================== */
static bool handle_transport_and_others(const ui_input_event_t *evt) {
    if (!evt->has_button || !evt->btn_pressed) return false;

    /* SHIFT + (PLAY/REC/STOP) : réservé */
    if (ui_input_shift_is_pressed() &&
        (evt->btn_id == UI_BTN_PLAY || evt->btn_id == UI_BTN_REC || evt->btn_id == UI_BTN_STOP)) {
        return true;
    }

    /* SHIFT + (+/-) : réservé */
    if (ui_input_shift_is_pressed() &&
        (evt->btn_id == UI_BTN_PLUS || evt->btn_id == UI_BTN_MINUS)) {
        return true;
    }

    /* SHIFT + BM1..BM4 : changement de cart réelle */
    if (ui_input_shift_is_pressed() &&
        (evt->btn_id == UI_BTN_PARAM1 || evt->btn_id == UI_BTN_PARAM2 ||
         evt->btn_id == UI_BTN_PARAM3 || evt->btn_id == UI_BTN_PARAM4)) {

        const ui_cart_spec_t* spec = NULL;
        if      (evt->btn_id == UI_BTN_PARAM1) spec = cart_registry_switch(CART1);
        else if (evt->btn_id == UI_BTN_PARAM2) spec = cart_registry_switch(CART2);
        else if (evt->btn_id == UI_BTN_PARAM3) spec = cart_registry_switch(CART3);
        else if (evt->btn_id == UI_BTN_PARAM4) spec = cart_registry_switch(CART4);

        if (spec) {
            if (ui_overlay_is_active()) ui_overlay_exit();
            ui_switch_cart(spec);
            /* Changer de cart sort du contexte Keys actif */
            s_keys_active = false;
            ui_led_backend_set_mode(UI_LED_MODE_NONE);
            ui_model_set_active_overlay_tag("");
            ui_mark_dirty();
            return true;
        }
    }

    return false;
}

/* ======================================================================
 * API publique
 * ====================================================================== */

void ui_shortcuts_init(void) {
    s_mute = MUTE_OFF;
    s_plus_down = false;
    s_last_shift = ui_input_shift_is_pressed();

    /* LED : au reset, mode visuel = NONE (steps off) */
    ui_led_backend_set_mode(UI_LED_MODE_NONE);

    s_keys_active = false;
    _restore_overlay_banner_tags();
}

void ui_shortcuts_reset(void) {
    ui_shortcuts_init();
}

bool ui_shortcuts_handle_event(const ui_input_event_t *evt) {
    if (handle_mute(evt)) return true;
    if (handle_overlays(evt)) return true;
    if (handle_transport_and_others(evt)) return true;
    return false;
}

bool ui_shortcuts_is_keys_active(void) {
    return s_keys_active;
}
