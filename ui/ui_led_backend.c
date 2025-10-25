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
#include "ch.h"
#include "brick_config.h"
#include "ui_led_backend.h"
#include "ui_led_palette.h"
#include "drv_leds_addr.h"
#include "ui_led_layout.h"
#include "ui_led_seq.h"   /* @ingroup ui_led_backend @ingroup ui_seq */
#include "core/ram_audit.h"

#ifdef UI_DEBUG_TRACE_LED_BACKEND
#include <stdio.h>
#define UI_LED_TRACE(...)                                                              \
    do { printf("[ui-led] "); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define UI_LED_TRACE(...)                                                              \
    do { (void)sizeof(#__VA_ARGS__); } while (0)
#endif

#ifndef NUM_STEPS
#define NUM_STEPS 16
#endif

/* ===== ÉTAT ===== */
static CCM_DATA bool     s_track_muted[NUM_STEPS];
UI_RAM_AUDIT(s_track_muted);
static CCM_DATA bool     s_track_pmutes[NUM_STEPS];
UI_RAM_AUDIT(s_track_pmutes);
static CCM_DATA bool     s_track_present[NUM_STEPS];
UI_RAM_AUDIT(s_track_present);
static CCM_DATA uint8_t  s_cart_tracks[4] = {4,4,4,4};
UI_RAM_AUDIT(s_cart_tracks);
static uint8_t           s_track_focus = 0U;
static bool     s_rec_active = false;
static ui_led_mode_t s_mode = UI_LED_MODE_NONE;

/* Keyboard */
static bool     s_kbd_omni = false;

/* Pré-calcul des couleurs accords Omnichord → stockées en Flash (.rodata). */
static const led_color_t k_omni_chord_colors[8] = {
    {0,   255,   0},
    {255,   0,   0},
    {0,   128, 128},
    {64,  255,   0},
    {255, 255,   0},
    {255,   0, 255},
    {0,   200, 200},
    {180,   0, 120},
};

typedef struct {
    ui_led_event_t event;
    uint8_t index;
    bool state;
} ui_led_backend_evt_t;

#ifndef UI_LED_BACKEND_QUEUE_CAPACITY
/* 16 tracks × (mute+pmute) ⇒ 32 événements : garder une marge pour éviter le drop. */
#define UI_LED_BACKEND_QUEUE_CAPACITY 64U
#endif

static CCM_DATA ui_led_backend_evt_t s_evt_queue[UI_LED_BACKEND_QUEUE_CAPACITY];
UI_RAM_AUDIT(s_evt_queue);
static uint8_t s_evt_head = 0U;
static uint8_t s_evt_tail = 0U;

#ifdef UI_LED_BACKEND_TESTING
static uint32_t s_queue_drop_count = 0U;
#endif

static inline uint8_t _queue_next(uint8_t idx) {
    return (uint8_t)((idx + 1U) % UI_LED_BACKEND_QUEUE_CAPACITY);
}

static void _queue_push_locked(const ui_led_backend_evt_t *evt) {
    uint8_t next_tail = _queue_next(s_evt_tail);
    if (next_tail == s_evt_head) {
        /* Saturation : drop le plus ancien pour éviter de bloquer. */
        s_evt_head = _queue_next(s_evt_head);
#ifdef UI_LED_BACKEND_TESTING
        s_queue_drop_count++;
#endif
        if (evt != NULL) {
            UI_LED_TRACE("queue_drop event=%d index=%u state=%d",
                         (int)evt->event,
                         (unsigned)evt->index,
                         evt->state ? 1 : 0);
        } else {
            UI_LED_TRACE("queue_drop event=unknown");
        }
    }
    s_evt_queue[s_evt_tail] = *evt;
    s_evt_tail = next_tail;
}

static bool _queue_pop_locked(ui_led_backend_evt_t *evt) {
    if (s_evt_head == s_evt_tail) {
        return false;
    }
    if (evt != NULL) {
        *evt = s_evt_queue[s_evt_head];
    }
    s_evt_head = _queue_next(s_evt_head);
    return true;
}

static void _apply_event(const ui_led_backend_evt_t *evt) {
    if (evt == NULL) {
        return;
    }

    switch (evt->event) {
        case UI_LED_EVENT_MUTE_STATE:
            if ((evt->index & 15u) < NUM_STEPS) {
                s_track_muted[evt->index & 15u] = evt->state;
            }
            break;

        case UI_LED_EVENT_PMUTE_STATE:
            if ((evt->index & 15u) < NUM_STEPS) {
                s_track_pmutes[evt->index & 15u] = evt->state;
            }
            break;

        case UI_LED_EVENT_CLOCK_TICK:
            /* SEQ : forward **absolu** (0..255) → le renderer applique son modulo [pages*16] */
            ui_led_seq_on_clock_tick(evt->index);
            break;

        case UI_LED_EVENT_STEP_STATE:
        case UI_LED_EVENT_PARAM_SELECT:
        default:
            break;
    }
}

static void _drain_event_queue(void) {
    ui_led_backend_evt_t evt;

    while (true) {
        chSysLock();
        const bool has_evt = _queue_pop_locked(&evt);
        chSysUnlock();
        if (!has_evt) {
            break;
        }
        _apply_event(&evt);
    }
}

/* ===== MAP step→LED physique ===== */
static inline int _led_index_for_step(uint8_t step) {
    if (step >= UI_LED_SEQ_STEP_COUNT) {
        return k_ui_led_seq_step_to_index[0];
    }
    return k_ui_led_seq_step_to_index[step];
}
static inline void _set_led(int idx, led_color_t col, led_mode_t mode) {
    drv_leds_addr_set(idx, col, mode);
}

/* ===== Rendu : MUTE (sans chenillard) ===== */
static inline led_color_t _cart_color(uint8_t cart_idx) {
    switch (cart_idx) {
    case 0:  return UI_LED_COL_CART1_ACTIVE;
    case 1:  return UI_LED_COL_CART2_ACTIVE;
    case 2:  return UI_LED_COL_CART3_ACTIVE;
    default: return UI_LED_COL_CART4_ACTIVE;
    }
}

static inline void _render_mute_mode(void) {
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const uint8_t cart_idx    = t / 4;
        const uint8_t pos_in_cart = t % 4;
        const int     led_idx     = _led_index_for_step(t);

        if ((pos_in_cart >= s_cart_tracks[cart_idx]) || !s_track_present[t]) {
            _set_led(led_idx, UI_LED_COL_OFF, LED_MODE_OFF);
            continue;
        }
        const bool muted   = s_track_muted[t];
        const bool preview = s_track_pmutes[t];
        bool future_muted  = muted;
        if (preview) {
            future_muted = !future_muted; // --- FIX: refléter l'état cible PMUTE immédiatement ---
        }

        if (future_muted) {
            _set_led(led_idx, UI_LED_COL_MUTE_RED, LED_MODE_ON);
            continue;
        }

        /* Couleur cart active, SANS accent de tick */
        _set_led(led_idx, _cart_color(cart_idx), LED_MODE_ON);
    }
}

static inline void _render_track_mode(void) {
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const uint8_t cart_idx    = t / 4;
        const uint8_t pos_in_cart = t % 4;
        const int     led_idx     = _led_index_for_step(t);

        if ((pos_in_cart >= s_cart_tracks[cart_idx]) || !s_track_present[t]) {
            _set_led(led_idx, UI_LED_COL_OFF, LED_MODE_OFF);
            continue;
        }

        if (t == (s_track_focus & 0x0FU)) {
            _set_led(led_idx, UI_LED_COL_SEQ_ACTIVE, LED_MODE_ON);
            continue;
        }

        if (s_track_muted[t]) {
            _set_led(led_idx, UI_LED_COL_MUTE_RED, LED_MODE_ON);
            continue;
        }

        _set_led(led_idx, _cart_color(cart_idx), LED_MODE_ON);
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
    for (uint8_t t = 0; t < NUM_STEPS; ++t) {
        const int led = _led_index_for_step(t);
        if ((t >= 4 && t <= 7) || (t >= 12 && t <= 15)) {
            _set_led(led, UI_LED_COL_KEY_BLUE_HI, LED_MODE_ON);
            continue;
        }
        if (t <= 3 || (t >= 8 && t <= 11)) {
            uint8_t chord_idx = (t <= 3) ? t : (uint8_t)(4 + (t - 8));
            _set_led(led, k_omni_chord_colors[chord_idx], LED_MODE_ON);
            continue;
        }
        _set_led(led, UI_LED_COL_OFF, LED_MODE_OFF);
    }
}

/* ===== API ===== */
void ui_led_backend_init(void) {
    memset(s_track_muted, 0, sizeof(s_track_muted));
    memset(s_track_pmutes, 0, sizeof(s_track_pmutes));
    memset(s_track_present, 0, sizeof(s_track_present));
    s_cart_tracks[0] = s_cart_tracks[1] = s_cart_tracks[2] = s_cart_tracks[3] = 4;
    s_rec_active = false;
    s_mode = UI_LED_MODE_NONE;
    s_kbd_omni = false;
    s_evt_head = s_evt_tail = 0U;
    s_track_focus = 0U;

    drv_leds_addr_init();
    /* NE PAS toucher au buffer physique ici : render() s’en charge. */

    /* SEQ : chenillard arrêté au reset */
    ui_led_seq_set_running(false);
}

void ui_led_backend_post_event(ui_led_event_t event, uint8_t index, bool state) {
    const ui_led_backend_evt_t evt = { event, index, state };

    chSysLock();
    _queue_push_locked(&evt);
    chSysUnlock();
}

void ui_led_backend_post_event_i(ui_led_event_t event, uint8_t index, bool state) {
    const ui_led_backend_evt_t evt = { event, index, state };

    chSysLockFromISR();
    _queue_push_locked(&evt);
    chSysUnlockFromISR();
}

void ui_led_backend_set_record_mode(bool active) { s_rec_active = active; }
void ui_led_backend_set_mode(ui_led_mode_t mode) {
    UI_LED_TRACE("set_mode %d->%d", (int)s_mode, (int)mode);
    s_mode = mode;
}
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks) {
    if (cart_idx > 3) return;
    if (tracks > 4) tracks = 4;
    s_cart_tracks[cart_idx] = tracks;
}
void ui_led_backend_set_keyboard_omnichord(bool enabled) { s_kbd_omni = enabled; }
void ui_led_backend_set_track_focus(uint8_t track_index) { s_track_focus = (track_index & 0x0FU); }
void ui_led_backend_set_track_present(uint8_t track_index, bool present) {
    if (track_index < NUM_STEPS) {
        s_track_present[track_index] = present;
    }
}

/* ===== Rendu ===== */
void ui_led_backend_refresh(void) {

    /* 0) Appliquer les événements accumulés (non bloquant). */
    _drain_event_queue();

    /* 1) Remplir l’état logique (pas d’accès au buffer physique ici) */
    switch (s_mode) {
        case UI_LED_MODE_MUTE:
            _render_mute_mode();
            break;
        case UI_LED_MODE_KEYBOARD:
            s_kbd_omni ? _render_keyboard_omnichord()
                       : _render_keyboard_normal();
            break;
        case UI_LED_MODE_TRACK:
            _render_track_mode();
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

#ifdef UI_LED_BACKEND_TESTING
uint32_t ui_led_backend_debug_queue_drops(void) { return s_queue_drop_count; }
ui_led_mode_t ui_led_backend_debug_get_mode(void) { return s_mode; }
bool ui_led_backend_debug_track_muted(uint8_t track) {
    return (track < NUM_STEPS) ? s_track_muted[track] : false;
}
const led_state_t *ui_led_backend_debug_led_state(void) { return drv_leds_addr_state; }
#endif
