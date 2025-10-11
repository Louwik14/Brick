/**
 * @file ui_task.c
 * @brief Thread principal de l’interface utilisateur Brick — délégation « shortcuts-centric ».
 * @ingroup ui
 *
 * @details
 * Orchestration en couches :
 *
 *   [ui_input] → [ui_task] → [ui_shortcuts] → [ui_controller / ui_overlay / ui_mute_backend]
 *                                     ↳ toutes les combinaisons (SHIFT+…)
 *
 * Rôles de ce thread :
 *  - Poller les entrées via @ref ui_input_poll (boutons + encodeurs). :contentReference[oaicite:4]{index=4}
 *  - Déléguer en priorité à @ref ui_shortcuts_handle_event pour MUTE/PMUTE, overlays, transport, etc.
 *  - Si l’évènement n’est pas consommé, relayer les événements simples au contrôleur :
 *      - Boutons menus/pages : @ref ui_on_button_menu / @ref ui_on_button_page. :contentReference[oaicite:5]{index=5}
 *      - Encodeurs : @ref ui_on_encoder (corrigé pour bipolarité dans ton contrôleur). :contentReference[oaicite:6]{index=6}
 *  - Déclencher le rendu conditionnel : @ref ui_is_dirty → @ref ui_render → @ref ui_clear_dirty. :contentReference[oaicite:7]{index=7}
 *
 * Le changement de cartouche « réelle » et la sortie d’overlays sont gérés dans `ui_shortcuts`.
 * Ce fichier ne contient **aucune** logique MUTE/PMUTE/overlay.
 */

#include "ch.h"
#include "hal.h"

#include <stdbool.h>
#include <stdint.h>

/* API UI de haut niveau (pas de drivers ici) */
#include "ui_task.h"
#include "ui_input.h"        /* ui_input_poll(), ui_input_shift_is_pressed() */
#include "ui_controller.h"   /* ui_init, ui_on_button_menu/page, ui_on_encoder, ui_get_state/cart, dirty */ /* :contentReference[oaicite:8]{index=8} */
#include "ui_renderer.h"     /* ui_render(), primitives de rendu */                                           /* :contentReference[oaicite:9]{index=9} */
#include "cart_registry.h"   /* cart_registry_get_active_id(), cart_registry_get_ui_spec() */                /* :contentReference[oaicite:10]{index=10} */

/* Nouveau module : détection centralisée des raccourcis SHIFT+… */
#include "ui_shortcuts.h"

/* ============================================================================
 * Configuration thread
 * ==========================================================================*/
#ifndef UI_TASK_STACK
#define UI_TASK_STACK  (1024)
#endif

#ifndef UI_TASK_PRIO
#define UI_TASK_PRIO   (NORMALPRIO)
#endif

#ifndef UI_TASK_POLL_MS
#define UI_TASK_POLL_MS (20)   /* attente max pour un événement bouton */
#endif

#ifndef UI_TASK_HEARTBEAT_MS
#define UI_TASK_HEARTBEAT_MS (500) /* rafraîchissement périodique (anti-stall) */
#endif

static THD_WORKING_AREA(waUI, UI_TASK_STACK);
static thread_t* s_ui_thread = NULL;

/* ============================================================================
 * Thread principal
 * ==========================================================================*/
static THD_FUNCTION(UIThread, arg) {
    (void)arg;
    chRegSetThreadName("UI");

    /* Initialisation contrôleur depuis la cartouche active */
    {
        cart_id_t active = cart_registry_get_active_id();
        const ui_cart_spec_t* init_spec = cart_registry_get_ui_spec(active);
        ui_init(init_spec); /* charge la spec + cycles BM déclaratifs */ /* :contentReference[oaicite:11]{index=11} */
    }

    /* Init du module de raccourcis (machine d’état MUTE, timers, etc.) */
    ui_shortcuts_init();

    ui_input_event_t evt;
    systime_t last_heartbeat = chVTGetSystemTimeX();

    for (;;) {
        /* ===== 1) Entrées ===== */
        const bool got = ui_input_poll(&evt, TIME_MS2I(UI_TASK_POLL_MS)); /* bouton et/ou encodeur */ /* :contentReference[oaicite:12]{index=12} */

        if (got) {
            /* 1.1) Déléguer en priorité aux raccourcis SHIFT+… */
            if (!ui_shortcuts_handle_event(&evt)) {
                /* 1.2) Non consommé → traiter les évènements simples ici */

                /* --- Boutons (press-only) --- */
                if (evt.has_button && evt.btn_pressed) {
                    switch (evt.btn_id) {
                        /* Menus (BM1..BM8) */
                        case UI_BTN_PARAM1: ui_on_button_menu(0);  break;
                        case UI_BTN_PARAM2: ui_on_button_menu(1);  break;
                        case UI_BTN_PARAM3: ui_on_button_menu(2);  break;
                        case UI_BTN_PARAM4: ui_on_button_menu(3);  break;
                        case UI_BTN_PARAM5: ui_on_button_menu(4);  break;
                        case UI_BTN_PARAM6: ui_on_button_menu(5);  break;
                        case UI_BTN_PARAM7: ui_on_button_menu(6);  break;
                        case UI_BTN_PARAM8: ui_on_button_menu(7);  break;

                        /* Pages (P1..P5) */
                        case UI_BTN_PAGE1:  ui_on_button_page(0);  break;
                        case UI_BTN_PAGE2:  ui_on_button_page(1);  break;
                        case UI_BTN_PAGE3:  ui_on_button_page(2);  break;
                        case UI_BTN_PAGE4:  ui_on_button_page(3);  break;
                        case UI_BTN_PAGE5:  ui_on_button_page(4);  break;

                        default: break; /* autres cas déjà gérés/consommés par ui_shortcuts */
                    }
                }

                /* --- Encodeurs --- */
                if (evt.has_encoder && evt.enc_delta != 0) {
                    ui_on_encoder((int)evt.encoder, (int)evt.enc_delta); /* bipolarité gérée côté contrôleur */ /* :contentReference[oaicite:13]{index=13} */
                }
            }
        }

        /* ===== 2) Rendu conditionnel ===== */
        const systime_t now = chVTGetSystemTimeX();
        const bool heartbeat = (now - last_heartbeat) >= TIME_MS2I(UI_TASK_HEARTBEAT_MS);
        if (heartbeat) last_heartbeat = now;

        if (ui_is_dirty() || heartbeat) {               /* dirty flag exposé par ui_controller */        /* :contentReference[oaicite:14]{index=14} */
            ui_render();                                /* wrapper qui appelle ui_draw_frame(st,cart) */  /* :contentReference[oaicite:15]{index=15} */
            ui_clear_dirty();                           /* reset du dirty flag */                         /* :contentReference[oaicite:16]{index=16} */
        }

        /* Cadence d'affichage ~60 FPS (sans bloquer l'acquisition d'entrées) */
        chThdSleepMilliseconds(16);
    }
}

/* ============================================================================
 * API publique
 * ==========================================================================*/

/**
 * @brief Démarre le thread UI (idempotent).
 * @ingroup ui
 */
void ui_task_start(void) {
    if (!s_ui_thread) {
        s_ui_thread = chThdCreateStatic(waUI, sizeof(waUI),
                                        UI_TASK_PRIO, UIThread, NULL);
    }
}

/**
 * @brief Indique si le thread UI est en cours d’exécution.
 * @return true si le thread a été créé.
 * @ingroup ui
 */
bool ui_task_is_running(void) {
    return s_ui_thread != NULL;
}
