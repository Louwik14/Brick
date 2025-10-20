/**
 * @file ui_led_seq.c
 * @brief Rendu LED du mode SEQ — playhead absolu, rendu stable (sans pulse).
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 *
 * @details
 * - Playhead **absolu** (0..total_span-1), n’auto-change pas la page visible.
 * - Affichage du playhead en LED_MODE_ON (un pas "plein", sans clignotement).
 * - Priorité d’affichage : playhead(blanc) > param_only(bleu) > active(vert) > off.
 * - Aucune dépendance à clock_manager (ticks injectés par ui_led_backend).
 * - IMPORTANT : `plock_selected_mask` est **UI-only**, il ne produit **aucune** couleur (plus de violet).
 */

#include <string.h>
#include "ui_led_seq.h"
#include "drv_leds_addr.h"
#include "ui_led_layout.h"
#include "ui_led_palette.h"

#ifndef SEQ_STEPS_PER_PAGE
#define SEQ_STEPS_PER_PAGE 16
#endif

typedef struct {
    seq_led_runtime_t rt;       /* Snapshot de la page visible */
    bool          running;      /* Lecture active */
    uint16_t      total_span;   /* pages * 16 (min 16, max 256) */
    uint16_t      play_abs;     /* Position absolue 0..total_span-1 */
    bool          has_tick;     /* Latch: vrai après 1er tick post-PLAY */
} seq_renderer_t;

static seq_renderer_t g;

/* ============== Mapping logique (0..15) → LED physique =================== */
static inline int _led_index_for_step(uint8_t s){
    if (s >= UI_LED_SEQ_STEP_COUNT) {
        return k_ui_led_seq_step_to_index[0];
    }
    return k_ui_led_seq_step_to_index[s];
}
static inline void _set_led_step(uint8_t s, led_color_t col, led_mode_t mode){
    drv_leds_addr_set(_led_index_for_step(s), col, mode);
}

/* ============================== API ===================================== */

void ui_led_seq_update_from_app(const seq_led_runtime_t *rt){
    if (!rt) return;
    g.rt = *rt;
    if (g.rt.steps_per_page == 0) g.rt.steps_per_page = SEQ_STEPS_PER_PAGE;
    if (g.rt.steps_per_page > 16) g.rt.steps_per_page = 16;
    if (g.total_span < g.rt.steps_per_page) g.total_span = g.rt.steps_per_page; /* garde-fou */
}

void ui_led_seq_set_total_span(uint16_t total_steps){
    if (total_steps < 16) total_steps = 16;
    if (total_steps > 256) total_steps = 256;
    g.total_span = total_steps;
    if (g.total_span) g.play_abs %= g.total_span; /* re-clamp si besoin */
}

void ui_led_seq_on_clock_tick(uint8_t step_index){
    if (g.total_span == 0) g.total_span = 64; /* défaut 4 pages */
    g.play_abs = ((uint16_t)step_index) % g.total_span;
    g.running  = true;
    g.has_tick = true; /* afficher le playhead uniquement à partir du 1er tick */
}

void ui_led_seq_set_running(bool running){
    g.running = running;
    /* À START/STOP : masquer le playhead jusqu'au 1er tick suivant */
    g.has_tick = false;
}

/* ============================== Rendu ==================================== */

static inline bool _is_playing_here(uint8_t local_idx){
    if (!g.has_tick) return false; /* évite l'effet "double" à PLAY */
    /* page du playhead (absolu) */
    uint8_t page = (g.total_span ? (g.play_abs / 16u) : 0u);
    if (page != g.rt.visible_page) return false;
    return ((g.play_abs % 16u) == local_idx);
}

static inline void _render_one(uint8_t s, bool is_playing_here){
    const seq_step_state_t *st = &g.rt.steps[s];

    if (g.running && is_playing_here) {
        /* 💡 Playhead SEQ : **stable**, pas de pulse */
        _set_led_step(s, UI_LED_COL_PLAYHEAD, LED_MODE_ON);
        return;
    }

    if (st->muted) {
        _set_led_step(s, UI_LED_COL_MUTE_RED, LED_MODE_ON);
        return;
    }
    if (st->automation) {
        _set_led_step(s, UI_LED_COL_SEQ_PARAM, LED_MODE_ON);
        return;
    }
    if (st->active) {
        _set_led_step(s, UI_LED_COL_SEQ_ACTIVE, LED_MODE_ON);
        return;
    }
    _set_led_step(s, UI_LED_COL_OFF, LED_MODE_OFF);
}

void ui_led_seq_render(void){
    if (g.rt.steps_per_page == 0) return;

    const uint8_t page = g.rt.visible_page;
    for (uint8_t s = 0; s < g.rt.steps_per_page; ++s) {
        const bool is_playing_here =
            (g.total_span && g.running && g.has_tick) &&
            ((g.play_abs / 16u) == page) &&
            ((g.play_abs % 16u) == s);

        _render_one(s, is_playing_here);
    }
}
