/**
 * @file ui_shortcuts.c
 * @brief Implémentation des raccourcis clavier (SHIFT + …) et MUTE/PMUTE.
 * @ingroup ui
 *
 * @details
 * Priorité de détection :
 *   1) MUTE (OFF/QUICK/PMUTE)
 *   2) OVERLAYS (SEQ/ARP via SHIFT+SEQ9/SEQ10, sortie via BM)
 *   3) TRANSPORT (réservé)
 *   4) AUTRES (réservés, ex. SHIFT+±, SHIFT+BM1..4)
 *
 * Accès MUTE depuis tous les modes (custom & parameter). Transitions sans double-tap :
 *  - Quick Mute : SHIFT + + press → actif tant que « + » maintenu (SHIFT peut être relâché)
 *  - Prepare Mute : pendant Quick, relâcher SHIFT puis le réappuyer → PMUTE (lock)
 *  - Commit : en PMUTE, appui « + » (sans SHIFT) → commit & sortie
 */

#include <string.h>
#include "ui_shortcuts.h"

#include "ui_controller.h"     /* ui_mark_dirty(), ui_switch_cart(), ui_get_cart() */
#include "ui_overlay.h"
#include "ui_model.h"
#include "ui_mute_backend.h"
#include "cart_registry.h"
#include "drv_buttons_map.h"

/* Specs SEQ / ARP */
#include "ui_seq_ui.h"
#include "ui_arp_ui.h"

/* ============================================================
 * Bannières overlay (copies peu profondes des specs réelles)
 * ============================================================ */
static ui_cart_spec_t s_seq_mode_spec_banner;
static ui_cart_spec_t s_seq_setup_spec_banner;

static ui_cart_spec_t s_arp_mode_spec_banner;
static ui_cart_spec_t s_arp_setup_spec_banner;

/* ============================================================
 * MUTE state machine
 * ============================================================ */
typedef enum {
    MUTE_OFF = 0,
    MUTE_QUICK,
    MUTE_PMUTE
} mute_state_t;

static mute_state_t s_mute = MUTE_OFF;
static bool         s_plus_down = false;     /* état de maintien du + */
static bool         s_last_shift = false;    /* pour détecter relâche/presse pendant QUICK */

/* ============================================================
 * Helpers locaux
 * ============================================================ */
static inline bool _is_bm(uint8_t btn) {
    return (btn == UI_BTN_PARAM1) || (btn == UI_BTN_PARAM2) ||
           (btn == UI_BTN_PARAM3) || (btn == UI_BTN_PARAM4) ||
           (btn == UI_BTN_PARAM5) || (btn == UI_BTN_PARAM6) ||
           (btn == UI_BTN_PARAM7) || (btn == UI_BTN_PARAM8);
}

static inline bool _is_seq1_8(uint8_t btn) {
    return (btn >= UI_BTN_SEQ1) && (btn <= UI_BTN_SEQ8);
}

/* --- Forcer l'affichage MUTE/PMUTE pendant un overlay actif ----------------
 * Le renderer préfère cart->overlay_tag si présent ; pour imposer "MUTE"/"PMUTE"
 * on met temporairement overlay_tag=NULL dans les bannières actives.
 * --------------------------------------------------------------------------*/
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
    }
}

static void _restore_overlay_banner_tags(void) {
    /* Quand on sort de MUTE/PMUTE, on rend la main aux labels overlay normaux. */
    s_seq_mode_spec_banner.overlay_tag   = "SEQ";
    s_seq_setup_spec_banner.overlay_tag  = "SEQ";
    s_arp_mode_spec_banner.overlay_tag   = "ARP";
    s_arp_setup_spec_banner.overlay_tag  = "ARP";
}

/* --- NOUVEAU : restaurer le tag persistant côté modèle en fonction
 *               du « custom mode » actif (SEQ/ARP) ------------------- */
static void _restore_overlay_tag_model_side(void) {
    const ui_custom_mode_t mode = ui_overlay_get_custom_mode();
    const char* tag =
        (mode == UI_CUSTOM_SEQ) ? "SEQ" :
        (mode == UI_CUSTOM_ARP) ? "ARP" : "";
    ui_model_set_active_overlay_tag(tag);
}

/* ============================================================
 * 1) MUTE — priorité maximale
 * ============================================================ */
static bool handle_mute(const ui_input_event_t *evt) {
    bool shift_now = ui_input_shift_is_pressed();

    /* Suivre l’état de + pour transitions temporelles */
    if (evt->has_button && evt->btn_id == UI_BTN_PLUS) {
        s_plus_down = evt->btn_pressed;
    }

    /* ---- Entrée QUICK : SHIFT + + pressé ---- */
    if (evt->has_button && evt->btn_pressed &&
        evt->btn_id == UI_BTN_PLUS && shift_now && s_mute == MUTE_OFF) {

        s_mute = MUTE_QUICK;
        s_last_shift = shift_now;
        ui_model_set_active_overlay_tag("MUTE");
        _neutralize_overlay_tag_for_mute_if_overlay_active(); /* forcer l'affichage du tag MUTE */
        ui_mark_dirty();
        return true; /* on consomme le + */
    }

    /* ---- Transition QUICK → PMUTE : relâcher SHIFT puis réappuyer, + toujours maintenu ---- */
    if (s_mute == MUTE_QUICK) {
        if (s_plus_down) {
            if (!shift_now && s_last_shift) {
                /* front descendant détecté */
                s_last_shift = false;
            } else if (shift_now && !s_last_shift) {
                /* front montant après relâche : passer en PMUTE */
                s_mute = MUTE_PMUTE;
                ui_model_set_active_overlay_tag("PMUTE");
                _neutralize_overlay_tag_for_mute_if_overlay_active(); /* forcer l'affichage du tag PMUTE */
                ui_mark_dirty();
                s_last_shift = true;
            }
        }
    }

    /* ---- QUICK : action live sur SEQ1..8 (press/release) ---- */
    if (s_mute == MUTE_QUICK && evt->has_button && _is_seq1_8(evt->btn_id)) {
        uint8_t track = (uint8_t)(evt->btn_id - UI_BTN_SEQ1);
        ui_mute_backend_apply(track, evt->btn_pressed);
        return true; /* consommé */
    }

    /* ---- Sortie QUICK : relâche du + ---- */
    if (s_mute == MUTE_QUICK && evt->has_button &&
        evt->btn_id == UI_BTN_PLUS && !evt->btn_pressed) {
        s_mute = MUTE_OFF;
        _restore_overlay_banner_tags();      /* rendre les labels SEQ/ARP à l’overlay */
        _restore_overlay_tag_model_side();   /* <<< RESTAURE "SEQ"/"ARP" selon mode actif */
        ui_mark_dirty();
        return true; /* consommé */
    }

    /* ---- PMUTE : préparation sur SEQ1..8 (front montant uniquement) ---- */
    if (s_mute == MUTE_PMUTE && evt->has_button && _is_seq1_8(evt->btn_id)) {
        if (evt->btn_pressed) {
            uint8_t track = (uint8_t)(evt->btn_id - UI_BTN_SEQ1);
            ui_mute_backend_toggle_prepare(track);
        }
        return true; /* consommé */
    }

    /* ---- PMUTE : commit via + (sans SHIFT) ---- */
    if (s_mute == MUTE_PMUTE && evt->has_button &&
        evt->btn_id == UI_BTN_PLUS && evt->btn_pressed && !shift_now) {
        ui_mute_backend_commit();
        s_mute = MUTE_OFF;
        s_plus_down = true;   /* + vient d’être pressé */
        _restore_overlay_banner_tags();      /* rendre les labels SEQ/ARP à l’overlay */
        _restore_overlay_tag_model_side();   /* <<< RESTAURE "SEQ"/"ARP" selon mode actif */
        ui_mark_dirty();
        return true; /* consommé */
    }

    return false; /* non concerné par MUTE */
}

/* ============================================================
 * 2) OVERLAYS — après MUTE
 * ============================================================ */
static bool handle_overlays(const ui_input_event_t *evt) {
    if (!evt->has_button || !evt->btn_pressed) {
        /* Overlays sont « press-only » */
        return false;
    }

    /* ---- Sortie overlay par BM ---- */
    if (ui_overlay_is_active() && _is_bm(evt->btn_id)) {
        ui_overlay_exit();
        /* laisser le BM se propager à la cart réelle → ne pas consommer */
        return false;
    }

    /* ---- SHIFT + SEQ9 → Overlay SEQ (toggle MODE/SETUP) ---- */
    if (ui_input_shift_is_pressed() && evt->btn_id == UI_BTN_SEQ9) {
        ui_overlay_prepare_banner(&seq_ui_spec, &seq_setup_ui_spec,
                                  &s_seq_mode_spec_banner, &s_seq_setup_spec_banner,
                                  ui_get_cart(), "SEQ");
        ui_overlay_set_custom_mode(UI_CUSTOM_SEQ);

        /* Si on est en MUTE/PMUTE, neutraliser tout de suite le tag bannière
           pour que "MUTE"/"PMUTE" s'affiche même en overlay. */
        if (s_mute == MUTE_QUICK || s_mute == MUTE_PMUTE) {
            s_seq_mode_spec_banner.overlay_tag  = NULL;
            s_seq_setup_spec_banner.overlay_tag = NULL;
        }

        if (!ui_overlay_is_active()) {
            ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
        } else if (ui_overlay_get_spec() == &s_seq_mode_spec_banner) {
            ui_overlay_switch_subspec(&s_seq_setup_spec_banner);
        } else if (ui_overlay_get_spec() == &s_seq_setup_spec_banner) {
            ui_overlay_switch_subspec(&s_seq_mode_spec_banner);
        } else {
            ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
        }
        ui_mark_dirty();
        return true;
    }

    /* ---- SHIFT + SEQ10 → Overlay ARP (toggle MODE/SETUP) ---- */
    if (ui_input_shift_is_pressed() && evt->btn_id == UI_BTN_SEQ10) {
        ui_overlay_prepare_banner(&arp_ui_spec, &arp_setup_ui_spec,
                                  &s_arp_mode_spec_banner, &s_arp_setup_spec_banner,
                                  ui_get_cart(), "ARP");
        ui_overlay_set_custom_mode(UI_CUSTOM_ARP);

        if (s_mute == MUTE_QUICK || s_mute == MUTE_PMUTE) {
            s_arp_mode_spec_banner.overlay_tag  = NULL;
            s_arp_setup_spec_banner.overlay_tag = NULL;
        }

        if (!ui_overlay_is_active()) {
            ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
        } else if (ui_overlay_get_spec() == &s_arp_mode_spec_banner) {
            ui_overlay_switch_subspec(&s_arp_setup_spec_banner);
        } else if (ui_overlay_get_spec() == &s_arp_setup_spec_banner) {
            ui_overlay_switch_subspec(&s_arp_mode_spec_banner);
        } else {
            ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
        }
        ui_mark_dirty();
        return true;
    }

    return false;
}

/* ============================================================
 * 3) TRANSPORT & AUTRES RÉSERVÉS
 * ============================================================ */
static bool handle_transport_and_others(const ui_input_event_t *evt) {
    if (!evt->has_button || !evt->btn_pressed) return false;

    /* SHIFT + PLAY/REC/STOP : réservés, on consomme pour centraliser */
    if (ui_input_shift_is_pressed() &&
        (evt->btn_id == UI_BTN_PLAY || evt->btn_id == UI_BTN_REC || evt->btn_id == UI_BTN_STOP)) {
        /* Réservé: pas d’action pour l’instant */
        return true;
    }

    /* SHIFT + +/- : réservés globaux → consommer (évite effets de bord) */
    if (ui_input_shift_is_pressed() &&
        (evt->btn_id == UI_BTN_PLUS || evt->btn_id == UI_BTN_MINUS)) {
        /* NB: SHIFT + PLUS est déjà traité par MUTE */
        return true;
    }

    /* SHIFT + BM1..BM4 → changement de cart « réelle » (press-only) */
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
            ui_mark_dirty();
            return true;
        }
    }

    return false;
}

/* ============================================================
 * API publique
 * ============================================================ */
void ui_shortcuts_init(void) {
    s_mute = MUTE_OFF;
    s_plus_down = false;
    s_last_shift = ui_input_shift_is_pressed();

    /* Par défaut, les bannières affichent leur tag natif (SEQ/ARP). */
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
