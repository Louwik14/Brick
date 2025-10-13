/**
 * @file ui_led_backend.c
 * @brief Backend LED adressable (SEQ/MUTE/KEYBOARD) — rendu unifié et atomique.
 * @ingroup ui
 *
 * @details
 * - Le backend ne manipule **pas** le buffer physique directement.
 * - Il remplit uniquement `drv_leds_addr_state[]` via `drv_leds_addr_set(...)`.
 * - Le **seul** endroit qui convertit `state[]` → `led_buffer[]` + envoi est
 *   `drv_leds_addr_render()`, appelé **une fois** à la fin de `ui_led_backend_refresh()`.
 * - Le playhead SEQ reçoit l’index **absolu** via `ui_led_seq_on_clock_tick()`.
 * - MUTE : pas de chenillard (aucun pulse de tick).
 */

#include <string.h>
#include "ui_led_backend.h"
#include "ui_led_palette.h"
#include "drv_leds_addr.h"
#include "ui_led_seq.h"   /* @ingroup ui_led_backend @ingroup ui_seq */

#ifndef NUM_STEPS
#define NUM_STEPS 16
#endif

/* ===== ÉTAT ===== */
static bool     s_track_muted[NUM_STEPS];
static uint8_t  s_cart_tracks[4] = {4,4,4,4};
static bool     s_rec_active = false;
static ui_led_mode_t s_mode = UI_LED_MODE_NONE;

/* Keyboard */
static bool     s_kbd_omni = false;

/* ===== MAP step→LED physique ===== */
static inline int _led_index_for_step(uint8_t step) {
    static const uint8_t idx[NUM_STEPS] = {
        LED_SEQ1, LED_SEQ2, LED_SEQ3, LED_SEQ4,
        LED_SEQ5, LED_SEQ6, LED_SEQ7, LED_SEQ8,
        LED_SEQ9, LED_SEQ10, LED_SEQ11, LED_SEQ12,
        LED_SEQ13, LED_SEQ14, LED_SEQ15, LED_SEQ16
    };
    return (step < NUM_STEPS) ? idx[step] : LED_SEQ1;
}
static inline void _set_led(int idx, led_color_t col, led_mode_t mode) {
    drv_leds_addr_set(idx, col, mode);
}

/* ===== Rendu : MUTE (sans chenillard) ===== */
static inline void _render_mute_mode(void) {
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const uint8_t cart_idx    = t / 4;
        const uint8_t pos_in_cart = t % 4;
        const int     led_idx     = _led_index_for_step(t);

        if (pos_in_cart >= s_cart_tracks[cart_idx]) {
            _set_led(led_idx, UI_LED_COL_OFF, LED_MODE_OFF);
            continue;
        }
        if (s_track_muted[t]) {
            _set_led(led_idx, UI_LED_COL_MUTE_RED, LED_MODE_ON);
            continue;
        }

        /* Couleur cart active, SANS accent de tick */
        led_color_t c =
            (cart_idx == 0) ? UI_LED_COL_CART1_ACTIVE :
            (cart_idx == 1) ? UI_LED_COL_CART2_ACTIVE :
            (cart_idx == 2) ? UI_LED_COL_CART3_ACTIVE :
                              UI_LED_COL_CART4_ACTIVE;
        _set_led(led_idx, c, LED_MODE_ON);
    }
}

/* ===== Rendu : KEYBOARD (identique) ===== */
static inline void _render_keyboard_normal(void) {
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const int led = _led_index_for_step(t);
        const bool second_row = (t >= 8);
        _set_led(led, second_row ? UI_LED_COL_KEY_BLUE_LO : UI_LED_COL_KEY_BLUE_HI, LED_MODE_ON);
    }
}
static inline void _render_keyboard_omnichord(void) {
    static const led_color_t chord_colors[8] = {
        UI_LED_COL_CHORD_1, UI_LED_COL_CHORD_2, UI_LED_COL_CHORD_3, UI_LED_COL_CHORD_4,
        UI_LED_COL_CHORD_5, UI_LED_COL_CHORD_6, UI_LED_COL_CHORD_7, UI_LED_COL_CHORD_8
    };
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const int led = _led_index_for_step(t);
        if ((t >= 4 && t <= 7) || (t >= 12 && t <= 15)) {
            _set_led(led, UI_LED_COL_KEY_BLUE_HI, LED_MODE_ON);
            continue;
        }
        if (t <= 3 || (t >= 8 && t <= 11)) {
            uint8_t chord_idx = (t <= 3) ? t : (uint8_t)(4 + (t - 8));
            _set_led(led, chord_colors[chord_idx], LED_MODE_ON);
            continue;
        }
        _set_led(led, UI_LED_COL_OFF, LED_MODE_OFF);
    }
}

/* ===== API ===== */
void ui_led_backend_init(void) {
    memset(s_track_muted, 0, sizeof(s_track_muted));
    s_cart_tracks[0] = s_cart_tracks[1] = s_cart_tracks[2] = s_cart_tracks[3] = 4;
    s_rec_active = false;
    s_mode = UI_LED_MODE_NONE;
    s_kbd_omni = false;

    drv_leds_addr_init();
    /* NE PAS toucher au buffer physique ici : render() s’en charge. */

    /* SEQ : chenillard arrêté au reset */
    ui_led_seq_set_running(false);
}

void ui_led_backend_process_event(ui_led_event_t event, uint8_t index, bool state) {
    switch (event) {
        case UI_LED_EVENT_MUTE_STATE:
        case UI_LED_EVENT_PMUTE_STATE:
            if ((index & 15u) < NUM_STEPS) s_track_muted[index & 15u] = state;
            break;

        case UI_LED_EVENT_CLOCK_TICK:
            /* SEQ : forward **absolu** (0..255) → le renderer applique son modulo [pages*16] */
            ui_led_seq_on_clock_tick(index);
            break;

        default:
            break;
    }
}

void ui_led_backend_set_record_mode(bool active) { s_rec_active = active; }
void ui_led_backend_set_mode(ui_led_mode_t mode) { s_mode = mode; }
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks) {
    if (cart_idx > 3) return;
    if (tracks > 4) tracks = 4;
    s_cart_tracks[cart_idx] = tracks;
}
void ui_led_backend_set_keyboard_omnichord(bool enabled) { s_kbd_omni = enabled; }

/* ===== Rendu ===== */
void ui_led_backend_refresh(void) {

    /* 1) Remplir l’état logique (pas d’accès au buffer physique ici) */
    switch (s_mode) {
        case UI_LED_MODE_MUTE:
            _render_mute_mode();
            break;
        case UI_LED_MODE_KEYBOARD:
            s_kbd_omni ? _render_keyboard_omnichord()
                       : _render_keyboard_normal();
            break;
        case UI_LED_MODE_SEQ:
            ui_led_seq_render();
            break;
        default:
            for (uint8_t t = 0; t < NUM_STEPS; ++t)
                _set_led(_led_index_for_step(t), UI_LED_COL_OFF, LED_MODE_OFF);
            break;
    }

    /* 2) Led REC globale (toujours via l’état logique) */
    _set_led(LED_REC, s_rec_active ? UI_LED_COL_REC_ACTIVE : UI_LED_COL_OFF, LED_MODE_ON);

    /* 3) Conversion state[] → buffer + envoi (unique point d’accès au buffer) */
    drv_leds_addr_render();
}
