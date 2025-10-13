/**
 * @file clock_manager.c
 * @brief Gestionnaire central du tempo et des signaux d’horloge MIDI / interne.
 *
 * Ce module unifie la gestion du **tempo**, du **déclenchement de pas (step)** et
 * du **routage MIDI Clock** :
 * - Source d’horloge interne ou externe (via MIDI)
 * - Conversion 24 PPQN → 1/16 (6 ticks MIDI par pas)
 * - Démarrage / arrêt synchronisé (Start/Stop/SongPos)
 * - Gestion du tempo via `midi_clock`
 *
 * @ingroup clock
 */

#include "clock_manager.h"
#include "midi_clock.h"
#include "midi.h"
#include "hal.h"   // chTimeUS2I()

/* ======================================================================
 *                              ÉTAT GLOBAL
 * ====================================================================== */

static clock_source_t   s_src          = CLOCK_SRC_INTERNAL;  /**< Source actuelle de l’horloge */
static uint32_t         s_tick_count   = 0;                    /**< Compteur interne de ticks MIDI (0..5) */
static uint32_t         s_step_idx_abs = 0;                    /**< Compteur absolu de steps 1/16 */

#ifndef CLOCK_MANAGER_MAX_OBSERVERS
#define CLOCK_MANAGER_MAX_OBSERVERS 4U
#endif

static clock_step_cb2_t s_step_observers[CLOCK_MANAGER_MAX_OBSERVERS];
static clock_step_handle_t s_legacy_handle = CLOCK_STEP_INVALID_HANDLE;

/* ======================================================================
 *                              FONCTIONS INTERNES
 * ====================================================================== */

/**
 * @brief Convertit le BPM courant en durées `systime_t` pour 1 tick et 1 step.
 * @param bpm Tempo actuel
 * @param[out] tick_st Durée d’1 tick MIDI (24 PPQN) en `systime_t`
 * @param[out] step_st Durée d’1 step (1/16 = 6 ticks) en `systime_t`
 */
static inline void compute_periods_st(float bpm, systime_t *tick_st, systime_t *step_st) {
    if (bpm < 0.5f) bpm = 120.0f; // garde-fou
    // Période d’1 tick en microsecondes : 60e6 / (bpm * 24)
    const float    tick_us_f = 60000000.0f / (bpm * 24.0f);
    const uint32_t tick_us   = (uint32_t)(tick_us_f + 0.5f);
    const systime_t t_st     = chTimeUS2I(tick_us);
    if (tick_st) *tick_st = t_st;
    if (step_st) *step_st = t_st * 6U;
}

/**
 * @brief Gère un tick MIDI (1/24) → conversion en steps 1/16.
 *
 * Tous les 6 ticks MIDI, déclenche le callback V2.
 * Appelée depuis le thread `midi_clock` (pas en ISR).
 */
static void handle_tick(void) {
    s_tick_count++;
    if (s_tick_count < 6U) {
        return;
    }

    // 6 ticks → 1 step
    s_tick_count = 0U;
    const systime_t now = chVTGetSystemTimeX();
    const float     bpm = midi_clock_get_bpm();

    systime_t tick_st, step_st;
    compute_periods_st(bpm, &tick_st, &step_st);

    clock_step_info_t info = {
        .now          = now,
        .step_idx_abs = s_step_idx_abs,
        .bpm          = bpm,
        .tick_st      = tick_st,
        .step_st      = step_st,
        .source       = (s_src == CLOCK_SRC_MIDI)
                          ? CLOCK_STEP_SOURCE_EXTERNAL
                          : CLOCK_STEP_SOURCE_INTERNAL
    };

    for (uint8_t i = 0; i < CLOCK_MANAGER_MAX_OBSERVERS; ++i) {
        clock_step_cb2_t cb = s_step_observers[i];
        if (cb) {
            cb(&info);
        }
    }

    // Incrément du compteur absolu de steps (après notification)
    s_step_idx_abs++;
}

/**
 * @brief Callback appelé à chaque tick MIDI (F8) par `midi_clock`.
 *
 * Redirige selon la source d’horloge active.
 * @note Appelé depuis le **thread** midi_clock (priorité haute), pas en ISR.
 */
static void on_midi_tick(void) {
    if (s_src == CLOCK_SRC_INTERNAL) {
        handle_tick();
    } else {
        // TODO: mode esclave externe → gestion d'une source MIDI Clock externe
        // (filtrage F8 in, resynchronisation, etc.)
    }
}

/* ======================================================================
 *                              API PUBLIQUE
 * ====================================================================== */

void clock_manager_init(clock_source_t src) {
    s_src          = src;
    s_tick_count   = 0U;
    s_step_idx_abs = 0U;
    for (uint8_t i = 0; i < CLOCK_MANAGER_MAX_OBSERVERS; ++i) {
        s_step_observers[i] = NULL;
    }
    s_legacy_handle = CLOCK_STEP_INVALID_HANDLE;

    midi_clock_init();
    midi_clock_register_tick_callback(on_midi_tick);
}

void clock_manager_set_source(clock_source_t src) { s_src = src; }

clock_source_t clock_manager_get_source(void) { return s_src; }

void clock_manager_set_bpm(float bpm) {
    if (s_src == CLOCK_SRC_INTERNAL) {
        midi_clock_set_bpm(bpm);
    }
}

float clock_manager_get_bpm(void) { return midi_clock_get_bpm(); }

void clock_manager_start(void) {
    if (s_src == CLOCK_SRC_INTERNAL) {
        // S’assurer que le premier step part immédiatement après le 1er F8
        s_tick_count   = 5U;
        s_step_idx_abs = 0U;

        midi_song_position(MIDI_DEST_USB, 0);
        midi_start(MIDI_DEST_USB);
        midi_clock_start();
    } else {
        // TODO: support futur du mode esclave externe
    }
}

void clock_manager_stop(void) {
    midi_stop(MIDI_DEST_USB);
    midi_clock_stop();
}

bool clock_manager_is_running(void) { return midi_clock_is_running(); }

clock_step_handle_t clock_manager_step_subscribe(clock_step_cb2_t cb) {
    if (!cb) {
        return CLOCK_STEP_INVALID_HANDLE;
    }
    for (uint8_t i = 0; i < CLOCK_MANAGER_MAX_OBSERVERS; ++i) {
        if (s_step_observers[i] == NULL) {
            s_step_observers[i] = cb;
            return (clock_step_handle_t)i;
        }
    }
    return CLOCK_STEP_INVALID_HANDLE;
}

void clock_manager_step_unsubscribe(clock_step_handle_t handle) {
    if (handle >= CLOCK_MANAGER_MAX_OBSERVERS) {
        return;
    }
    s_step_observers[handle] = NULL;
    if (s_legacy_handle == handle) {
        s_legacy_handle = CLOCK_STEP_INVALID_HANDLE;
    }
}

void clock_manager_register_step_callback2(clock_step_cb2_t cb) {
    if (s_legacy_handle != CLOCK_STEP_INVALID_HANDLE) {
        clock_manager_step_unsubscribe(s_legacy_handle);
        s_legacy_handle = CLOCK_STEP_INVALID_HANDLE;
    }
    if (cb) {
        s_legacy_handle = clock_manager_step_subscribe(cb);
    }
}
