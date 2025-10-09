/**
 * @file ui_task.c
 * @brief Thread principal de l’interface utilisateur Brick.
 * @ingroup ui
 *
 * @details
 * Orchestration Entrées (ui_input) → Contrôleur (ui_controller) → Rendu (ui_renderer).
 * Gère aussi le système d’OVERLAY générique (modes custom SEQ, ARP…)
 * qui s’appuie sur le module `ui_overlay.[ch]`.
 *
 * Chaque overlay (ex: SEQ, ARP) :
 *  - sauvegarde l’état et la cart “réelle” avant de basculer,
 *  - propose plusieurs sous-specs (MODE / SETUP),
 *  - se ferme proprement lorsqu’un bouton de menu (BM) est pressé.
 *
 * L’architecture reste 100 % UI, sans accès direct au bus ni aux drivers.
 */

#include "ch.h"
#include "hal.h"

#include "ui_task.h"
#include "ui_input.h"
#include "ui_renderer.h"
#include "ui_controller.h"
#include "cart_registry.h"
#include "ui_overlay.h"
#include "ui_seq_ui.h"     /* seq_ui_spec / seq_setup_ui_spec */
#include "ui_arp_ui.h"     /* arp_ui_spec / arp_setup_ui_spec */
#include "drv_buttons_map.h"

#include <string.h>

/* ============================================================
 * Configuration thread
 * ============================================================ */
#ifndef UI_TASK_STACK
#define UI_TASK_STACK 1024
#endif

#ifndef UI_TASK_PRIO
#define UI_TASK_PRIO (NORMALPRIO)
#endif

static THD_WORKING_AREA(waUI, UI_TASK_STACK);
static thread_t* s_ui_thread = NULL;

/* ============================================================
 * Bannières overlay (copies peu profondes des specs réelles)
 * ============================================================ */
static ui_cart_spec_t s_seq_mode_spec_banner;
static ui_cart_spec_t s_seq_setup_spec_banner;

static ui_cart_spec_t s_arp_mode_spec_banner;
static ui_cart_spec_t s_arp_setup_spec_banner;

/* ============================================================
 * Thread principal UI
 * ============================================================ */
/**
 * @brief Thread principal de l’interface utilisateur Brick.
 * @details
 * Gère :
 *  - la lecture des entrées (ui_input)
 *  - la navigation / logique (ui_controller)
 *  - le rendu (ui_renderer)
 *  - et les overlays custom (SEQ, ARP…)
 *
 * Les overlays sont exclusifs : activer l’un ferme automatiquement le précédent.
 * La sortie d’un overlay se fait par appui sur un BM (avec ou sans SHIFT).
 */
static THD_FUNCTION(UIThread, arg) {
    (void)arg;
    chRegSetThreadName("UI");

    /* Init contrôleur UI à partir du registre cartouches */
    {
        cart_id_t active = cart_registry_get_active_id();
        const ui_cart_spec_t* init_spec = cart_registry_get_ui_spec(active);
        ui_init(init_spec);
    }

    ui_input_event_t evt;
    systime_t last_heartbeat = chVTGetSystemTimeX();
    const systime_t heartbeat_interval = TIME_MS2I(500); // 2 Hz

    while (true) {
        /* --------- Entrées --------- */
        if (ui_input_poll(&evt, TIME_MS2I(20))) {

            /* --- Boutons --- */
            if (evt.has_button && evt.btn_pressed) {

                /* ============================================================
                 * SHIFT + SEQ9 → Overlay SEQ (Mode/Setup)
                 * ============================================================ */
                if (ui_input_shift_is_pressed() && evt.btn_id == UI_BTN_SEQ9) {
                    ui_overlay_prepare_banner(&seq_ui_spec, &seq_setup_ui_spec,
                                              &s_seq_mode_spec_banner, &s_seq_setup_spec_banner,
                                              ui_get_cart(), "SEQ");
                    ui_overlay_set_custom_mode(UI_CUSTOM_SEQ);

                    if (!ui_overlay_is_active()) {
                        ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
                    } else if (ui_overlay_get_spec() == &s_seq_mode_spec_banner) {
                        ui_overlay_switch_subspec(&s_seq_setup_spec_banner);
                    } else if (ui_overlay_get_spec() == &s_seq_setup_spec_banner) {
                        ui_overlay_switch_subspec(&s_seq_mode_spec_banner);
                    } else {
                        /* Un autre overlay était actif → remplacer par SEQ */
                        ui_overlay_enter(UI_OVERLAY_SEQ, &s_seq_mode_spec_banner);
                    }
                    continue; /* évènement consommé */
                }

                /* ============================================================
                 * SHIFT + BS10 → Overlay ARP (Mode/Setup)
                 * ============================================================ */
                if (ui_input_shift_is_pressed() && evt.btn_id == UI_BTN_SEQ10) {
                    ui_overlay_prepare_banner(&arp_ui_spec, &arp_setup_ui_spec,
                                              &s_arp_mode_spec_banner, &s_arp_setup_spec_banner,
                                              ui_get_cart(), "ARP");
                    ui_overlay_set_custom_mode(UI_CUSTOM_ARP);

                    if (!ui_overlay_is_active()) {
                        ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
                    } else if (ui_overlay_get_spec() == &s_arp_mode_spec_banner) {
                        ui_overlay_switch_subspec(&s_arp_setup_spec_banner);
                    } else if (ui_overlay_get_spec() == &s_arp_setup_spec_banner) {
                        ui_overlay_switch_subspec(&s_arp_mode_spec_banner);
                    } else {
                        /* Un autre overlay (ex. SEQ) était actif → remplacer par ARP */
                        ui_overlay_enter(UI_OVERLAY_ARP, &s_arp_mode_spec_banner);
                    }
                    continue; /* évènement consommé */
                }

                /* ============================================================
                 * Sortie overlay : si un BM est pressé (avec/sans SHIFT),
                 * on QUITTE d’abord l’overlay puis on traite le BM réel.
                 * ============================================================ */
                if (ui_overlay_is_active()) {
                    bool is_bm =
                        (evt.btn_id == UI_BTN_PARAM1) || (evt.btn_id == UI_BTN_PARAM2) ||
                        (evt.btn_id == UI_BTN_PARAM3) || (evt.btn_id == UI_BTN_PARAM4) ||
                        (evt.btn_id == UI_BTN_PARAM5) || (evt.btn_id == UI_BTN_PARAM6) ||
                        (evt.btn_id == UI_BTN_PARAM7) || (evt.btn_id == UI_BTN_PARAM8);
                    if (is_bm) {
                        ui_overlay_exit();
                        /* ne pas 'continue;' → on laisse le BM s’exécuter sur la cart réelle */
                    }
                }

                /* ============================================================
                 * SHIFT + BM1..BM4 → changement de cart “réelle” (classique)
                 * En changeant de cart, on purge tout overlay en cours.
                 * ============================================================ */
                if (ui_input_shift_is_pressed()) {
                    const ui_cart_spec_t* spec = NULL;

                    if      (evt.btn_id == UI_BTN_PARAM1) spec = cart_registry_switch(CART1);
                    else if (evt.btn_id == UI_BTN_PARAM2) spec = cart_registry_switch(CART2);
                    else if (evt.btn_id == UI_BTN_PARAM3) spec = cart_registry_switch(CART3);
                    else if (evt.btn_id == UI_BTN_PARAM4) spec = cart_registry_switch(CART4);

                    if (spec) {
                        if (ui_overlay_is_active()) ui_overlay_exit();
                        ui_switch_cart(spec);
                        ui_mark_dirty();
                        continue;
                    }
                }

                /* ============================================================
                 * Menus (BM1..BM8) — cycles gérés par ui_controller
                 * ============================================================ */
                if      (evt.btn_id == UI_BTN_PARAM1) ui_on_button_menu(0);
                else if (evt.btn_id == UI_BTN_PARAM2) ui_on_button_menu(1);
                else if (evt.btn_id == UI_BTN_PARAM3) ui_on_button_menu(2);
                else if (evt.btn_id == UI_BTN_PARAM4) ui_on_button_menu(3);
                else if (evt.btn_id == UI_BTN_PARAM5) ui_on_button_menu(4);
                else if (evt.btn_id == UI_BTN_PARAM6) ui_on_button_menu(5);
                else if (evt.btn_id == UI_BTN_PARAM7) ui_on_button_menu(6);
                else if (evt.btn_id == UI_BTN_PARAM8) ui_on_button_menu(7);

                /* ============================================================
                 * Pages (P1..P5)
                 * ============================================================ */
                else if (evt.btn_id == UI_BTN_PAGE1) ui_on_button_page(0);
                else if (evt.btn_id == UI_BTN_PAGE2) ui_on_button_page(1);
                else if (evt.btn_id == UI_BTN_PAGE3) ui_on_button_page(2);
                else if (evt.btn_id == UI_BTN_PAGE4) ui_on_button_page(3);
                else if (evt.btn_id == UI_BTN_PAGE5) ui_on_button_page(4);

                ui_mark_dirty();
            }

            /* --- Encodeurs --- */
            if (evt.has_encoder && evt.enc_delta != 0) {
                ui_on_encoder(evt.encoder, evt.enc_delta);
                ui_mark_dirty();
            }
        }

        /* --------- Rendu --------- */
        bool heartbeat = (chVTGetSystemTimeX() - last_heartbeat) >= heartbeat_interval;
        if (heartbeat) last_heartbeat = chVTGetSystemTimeX();

        if (ui_is_dirty() || heartbeat) {
            ui_draw_frame(ui_get_cart(), ui_get_state());
            ui_clear_dirty();
        }

        chThdSleepMilliseconds(16); // ~60 FPS
    }
}

/* ============================================================
 * Lancement / état du thread UI
 * ============================================================ */

/** @brief Lance le thread UI s’il n’est pas déjà actif. */
void ui_task_start(void) {
    if (!s_ui_thread) {
        s_ui_thread = chThdCreateStatic(waUI, sizeof(waUI),
                                        UI_TASK_PRIO, UIThread, NULL);
    }
}

/** @brief Indique si le thread UI est en cours d’exécution. */
bool ui_task_is_running(void) {
    return s_ui_thread != NULL;
}
