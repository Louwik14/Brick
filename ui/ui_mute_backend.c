/**
 * @file ui_mute_backend.c
 * @brief Implémentation du backend MUTE / PMUTE (toggle) pour l'UI Brick.
 * @ingroup ui
 *
 * @details
 * - Espace de 16 tracks : 4 cartouches × 4 (mapping pris en charge côté LED).
 * - QUICK MUTE : bascule (toggle) l'état d'une track à l'appui (SEQ1..8).
 * - PMUTE : bascule l'état préparé; commit applique, cancel annule.
 * - Rendu LED : @ref ui_led_backend reçoit les événements et affiche :
 *      - muté   → rouge
 *      - actif  → couleur cartouche (en mode MUTE uniquement)
 *
 * Aucune dépendance circulaire : ce module publie vers ui_led_backend
 * sans rien exiger en retour.
 */

#include "ui_mute_backend.h"
#include "ui_led_backend.h"

#include <string.h>

#ifndef NUM_TRACKS
#define NUM_TRACKS 16
#endif

/* ======================================================================
 * ÉTAT INTERNE
 * ====================================================================== */

static bool s_muted[NUM_TRACKS];         /* état MUTE réel par track */
static bool s_pmute_prepare[NUM_TRACKS]; /* état préparé PMUTE par track */

/* ======================================================================
 * OUTILS INTERNES
 * ====================================================================== */

static inline bool _valid(uint8_t t) { return t < NUM_TRACKS; }

/* ======================================================================
 * API
 * ====================================================================== */

void ui_mute_backend_init(void) {
    memset(s_muted, 0, sizeof(s_muted));
    memset(s_pmute_prepare, 0, sizeof(s_pmute_prepare));
}

void ui_mute_backend_apply(uint8_t track, bool mute) {
    if (!_valid(track)) return;
    s_muted[track] = mute;
    /* Visuel immédiat : MUTE_STATE = true/false */
    ui_led_backend_post_event(UI_LED_EVENT_MUTE_STATE, track, s_muted[track]);
}

void ui_mute_backend_toggle(uint8_t track) {
    if (!_valid(track)) return;
    s_muted[track] = !s_muted[track];
    ui_led_backend_post_event(UI_LED_EVENT_MUTE_STATE, track, s_muted[track]);
}

void ui_mute_backend_toggle_prepare(uint8_t track) {
    if (!_valid(track)) return;
    s_pmute_prepare[track] = !s_pmute_prepare[track];
    /* PMUTE = même rendu visuel que MUTE côté LEDs */
    ui_led_backend_post_event(UI_LED_EVENT_PMUTE_STATE, track, s_pmute_prepare[track]);
}

void ui_mute_backend_publish_state(void) {
    // --- FIX: réinitialiser l'état LED PMUTE/MUTE à chaque entrée dans le mode ---
    for (uint8_t i = 0; i < NUM_TRACKS; ++i) {
        ui_led_backend_post_event(UI_LED_EVENT_MUTE_STATE, i, s_muted[i]);
        ui_led_backend_post_event(UI_LED_EVENT_PMUTE_STATE, i, s_pmute_prepare[i]);
    }
}

void ui_mute_backend_commit(void) {
    /* Applique toutes les préparations en inversant l'état réel */
    for (uint8_t i = 0; i < NUM_TRACKS; ++i) {
        if (s_pmute_prepare[i]) {
            s_muted[i] = !s_muted[i];
            /* Publier l'état réel mis à jour */
            ui_led_backend_post_event(UI_LED_EVENT_MUTE_STATE, i, s_muted[i]);

            /* Nettoyer le flag préparé + notifier PMUTE=false */
            s_pmute_prepare[i] = false;
            ui_led_backend_post_event(UI_LED_EVENT_PMUTE_STATE, i, false);
        }
    }
}

void ui_mute_backend_cancel(void) {
    /* Annule toutes les préparations en cours */
    for (uint8_t i = 0; i < NUM_TRACKS; ++i) {
        if (s_pmute_prepare[i]) {
            s_pmute_prepare[i] = false;
            ui_led_backend_post_event(UI_LED_EVENT_PMUTE_STATE, i, false);
        }
    }
}

void ui_mute_backend_clear(void) {
    for (uint8_t i = 0; i < NUM_TRACKS; ++i) {
        if (s_pmute_prepare[i]) {
            s_pmute_prepare[i] = false;
            ui_led_backend_post_event(UI_LED_EVENT_PMUTE_STATE, i, false);
        }
    }
}

/* ======================================================================
 * GETTERS (optionnels)
 * ====================================================================== */

bool ui_mute_backend_is_muted(uint8_t track) {
    return _valid(track) ? s_muted[track] : false;
}

bool ui_mute_backend_is_prepared(uint8_t track) {
    return _valid(track) ? s_pmute_prepare[track] : false;
}
