/**
 * @file seq_led_bridge.c
 * @brief Sequencer LED bridge relying on seq_model pattern snapshots.
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */

#include <string.h>

#include "core/seq/seq_model.h"
#include "seq_led_bridge.h"

#ifndef SEQ_MAX_PAGES
#define SEQ_MAX_PAGES 16U
#endif
#ifndef SEQ_DEFAULT_PAGES
#define SEQ_DEFAULT_PAGES 4U
#endif
#ifndef SEQ_LED_BRIDGE_STEPS_PER_PAGE
#define SEQ_LED_BRIDGE_STEPS_PER_PAGE 16U
#endif

typedef struct {
    seq_model_pattern_t pattern;       /**< Backing sequencer pattern (64 steps). */
    uint16_t            page_hold_mask[SEQ_MAX_PAGES]; /**< Held-step mask per page (UI only). */
    uint16_t            preview_mask;  /**< Cached mask for the visible page. */
    seq_runtime_t       rt;            /**< Runtime payload consumed by ui_led_seq. */
    uint8_t             max_pages;     /**< Number of usable pages. */
    uint8_t             visible_page;  /**< Currently focused page. */
    uint16_t            total_span;    /**< Pattern span exposed to LEDs (pages × 16). */
    uint8_t             last_note;     /**< Last armed note used for quick steps. */
} seq_led_bridge_state_t;

static seq_led_bridge_state_t g;

/* ===== Helpers ============================================================ */
static inline uint16_t _page_base(uint8_t page) {
    return (uint16_t)page * SEQ_LED_BRIDGE_STEPS_PER_PAGE;
}

static inline uint16_t _clamp_total_span(uint16_t span) {
    if (span < SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        span = SEQ_LED_BRIDGE_STEPS_PER_PAGE;
    }
    if (span > SEQ_MODEL_STEPS_PER_PATTERN) {
        span = SEQ_MODEL_STEPS_PER_PATTERN;
    }
    return span;
}

static inline uint8_t _clamp_page(uint8_t page) {
    if (g.max_pages == 0U) {
        return 0U;
    }
    if (page >= g.max_pages) {
        page = (uint8_t)(g.max_pages - 1U);
    }
    return page;
}

static inline bool _valid_step_index(uint16_t absolute) {
    return (absolute < g.total_span) && (absolute < SEQ_MODEL_STEPS_PER_PATTERN);
}

static inline seq_model_step_t *_step_from_page(uint8_t local_step) {
    uint16_t absolute = _page_base(g.visible_page) + (uint16_t)local_step;
    if (!_valid_step_index(absolute)) {
        return NULL;
    }
    return &g.pattern.steps[absolute];
}

static inline void _clear_step_voices(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }
    for (size_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        seq_model_voice_t voice = step->voices[v];
        voice.state = SEQ_MODEL_VOICE_DISABLED;
        voice.velocity = 0U;
        step->voices[v] = voice;
    }
}

static void _ensure_placeholder_plock(seq_model_step_t *step) {
    if ((step == NULL) || (step->plock_count > 0U)) {
        return;
    }
    seq_model_plock_t placeholder = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = 0,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE
    };
    (void)seq_model_step_add_plock(step, &placeholder);
}

static void _update_preview_mask(void) {
    if (g.visible_page >= SEQ_MAX_PAGES) {
        g.preview_mask = 0U;
        return;
    }
    g.preview_mask = g.page_hold_mask[g.visible_page];
    g.rt.plock_selected_mask = g.preview_mask;
}

static void _rebuild_runtime_from_pattern(void) {
    memset(&g.rt, 0, sizeof(g.rt));
    g.rt.visible_page = g.visible_page;
    g.rt.steps_per_page = SEQ_LED_BRIDGE_STEPS_PER_PAGE;
    g.rt.plock_selected_mask = g.preview_mask;

    const uint16_t base = _page_base(g.visible_page);
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        const uint16_t absolute = base + (uint16_t)local;
        seq_step_state_t *dst = &g.rt.steps[local];

        if (!_valid_step_index(absolute)) {
            dst->active = false;
            dst->recorded = false;
            dst->param_only = false;
            continue;
        }

        const seq_model_step_t *src = &g.pattern.steps[absolute];
        const bool has_voice = seq_model_step_has_active_voice(src);
        const bool held = ((g.preview_mask >> local) & 0x1U) != 0U;
        const bool has_plock = (src->plock_count > 0U);

        dst->active = has_voice;
        dst->recorded = has_voice;
        dst->param_only = (!has_voice) && (has_plock || held);
    }

    ui_led_seq_set_total_span(g.total_span);
    ui_led_seq_update_from_app(&g.rt);
}

static void _publish_runtime(void) {
    _update_preview_mask();
    _rebuild_runtime_from_pattern();
}

/* ===== API =============================================================== */
void seq_led_bridge_init(void) {
    memset(&g, 0, sizeof(g));
    seq_model_pattern_init(&g.pattern);
    g.last_note = 60U;

    g.max_pages = (SEQ_DEFAULT_PAGES > SEQ_MAX_PAGES) ? SEQ_MAX_PAGES : SEQ_DEFAULT_PAGES;
    g.total_span = _clamp_total_span((uint16_t)g.max_pages * SEQ_LED_BRIDGE_STEPS_PER_PAGE);
    if (g.max_pages == 0U) {
        g.max_pages = 1U;
    }
    g.visible_page = 0U;

    _publish_runtime();
}

void seq_led_bridge_publish(void) {
    _publish_runtime();
}

void seq_led_bridge_set_max_pages(uint8_t max_pages) {
    if (max_pages == 0U) {
        max_pages = 1U;
    }
    if (max_pages > SEQ_MAX_PAGES) {
        max_pages = SEQ_MAX_PAGES;
    }

    const uint16_t span = _clamp_total_span((uint16_t)max_pages * SEQ_LED_BRIDGE_STEPS_PER_PAGE);
    g.max_pages = (uint8_t)((span + SEQ_LED_BRIDGE_STEPS_PER_PAGE - 1U) / SEQ_LED_BRIDGE_STEPS_PER_PAGE);
    g.total_span = span;

    for (uint8_t p = g.max_pages; p < SEQ_MAX_PAGES; ++p) {
        g.page_hold_mask[p] = 0U;
    }

    g.visible_page = _clamp_page(g.visible_page);
    _publish_runtime();
}

void seq_led_bridge_set_total_span(uint16_t total_steps) {
    total_steps = _clamp_total_span(total_steps);
    uint8_t pages = (uint8_t)((total_steps + SEQ_LED_BRIDGE_STEPS_PER_PAGE - 1U) / SEQ_LED_BRIDGE_STEPS_PER_PAGE);
    if (pages == 0U) {
        pages = 1U;
    }
    if (pages > SEQ_MAX_PAGES) {
        pages = SEQ_MAX_PAGES;
    }

    g.max_pages = pages;
    g.total_span = (uint16_t)pages * SEQ_LED_BRIDGE_STEPS_PER_PAGE;

    for (uint8_t p = g.max_pages; p < SEQ_MAX_PAGES; ++p) {
        g.page_hold_mask[p] = 0U;
    }

    g.visible_page = _clamp_page(g.visible_page);
    _publish_runtime();
}

void seq_led_bridge_page_next(void) {
    if (g.max_pages == 0U) {
        return;
    }
    g.visible_page = (uint8_t)((g.visible_page + 1U) % g.max_pages);
    _publish_runtime();
}

void seq_led_bridge_page_prev(void) {
    if (g.max_pages == 0U) {
        return;
    }
    g.visible_page = (uint8_t)((g.visible_page + g.max_pages - 1U) % g.max_pages);
    _publish_runtime();
}

void seq_led_bridge_set_visible_page(uint8_t page) {
    g.visible_page = _clamp_page(page);
    _publish_runtime();
}

uint8_t seq_led_bridge_get_visible_page(void) {
    return g.visible_page;
}

uint8_t seq_led_bridge_get_max_pages(void) {
    return g.max_pages;
}

void seq_led_bridge_step_clear(uint8_t i) {
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }
    seq_model_step_init(step);
    _clear_step_voices(step);
    seq_model_step_clear_plocks(step);
    seq_model_gen_bump(&g.pattern.generation);
}

void seq_led_bridge_step_set_voice(uint8_t i, uint8_t voice_idx, uint8_t pitch, uint8_t velocity) {
    if (voice_idx >= SEQ_MODEL_VOICES_PER_STEP) {
        return;
    }
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }

    seq_model_voice_t voice = step->voices[voice_idx];
    voice.note = pitch;
    voice.velocity = velocity;
    voice.state = (velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
    step->voices[voice_idx] = voice;
    if ((voice.state == SEQ_MODEL_VOICE_ENABLED) && (voice.velocity > 0U)) {
        g.last_note = voice.note;
    }
    seq_model_gen_bump(&g.pattern.generation);
}

void seq_led_bridge_step_set_has_plock(uint8_t i, bool on) {
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }

    if (on) {
        const uint16_t before = step->plock_count;
        _ensure_placeholder_plock(step);
        if (step->plock_count != before) {
            seq_model_gen_bump(&g.pattern.generation);
        }
    } else if (step->plock_count > 0U) {
        seq_model_step_clear_plocks(step);
        seq_model_gen_bump(&g.pattern.generation);
    }
}

void seq_led_bridge_quick_toggle_step(uint8_t i) {
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }

    const bool was_on = seq_model_step_has_active_voice(step) || (step->plock_count > 0U);
    if (was_on) {
        seq_led_bridge_step_clear(i);
    } else {
        seq_model_step_init_default(step, g.last_note);
        const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);
        if (voice != NULL) {
            g.last_note = voice->note;
        }
        seq_model_gen_bump(&g.pattern.generation);
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_set_step_param_only(uint8_t i, bool on) {
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }

    if (on) {
        seq_model_step_make_automate(step);
        _ensure_placeholder_plock(step);
        seq_model_gen_bump(&g.pattern.generation);
    } else if (step->plock_count > 0U) {
        seq_model_step_clear_plocks(step);
        seq_model_gen_bump(&g.pattern.generation);
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_on_play(void) {
    ui_led_seq_set_running(true);
}

void seq_led_bridge_on_stop(void) {
    ui_led_seq_set_running(false);
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = 0U;
    }
    g.preview_mask = 0U;
    seq_led_bridge_publish();
}

void seq_led_bridge_set_plock_mask(uint16_t mask) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = mask;
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_add(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    g.page_hold_mask[g.visible_page] |= (1u << i);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_remove(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    g.page_hold_mask[g.visible_page] &= (uint16_t)~(1u << i);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_clear(void) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = 0U;
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_begin_plock_preview(uint16_t held_mask) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = held_mask & 0xFFFFu;
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_apply_plock_param(uint8_t param_id, int32_t delta, uint16_t held_mask) {
    (void)param_id;
    (void)delta;

    if (g.visible_page >= SEQ_MAX_PAGES) {
        return;
    }

    bool mutated = false;
    for (uint8_t i = 0U; i < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++i) {
        if ((held_mask & (1u << i)) == 0U) {
            continue;
        }
        seq_model_step_t *step = _step_from_page(i);
        if (step == NULL) {
            continue;
        }
        const uint16_t before = step->plock_count;
        _ensure_placeholder_plock(step);
        mutated |= (step->plock_count != before);
    }

    if (mutated) {
        seq_model_gen_bump(&g.pattern.generation);
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_end_plock_preview(void) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = 0U;
    }
    seq_led_bridge_publish();
}

seq_model_pattern_t *seq_led_bridge_access_pattern(void) {
    return &g.pattern;
}

const seq_model_pattern_t *seq_led_bridge_get_pattern(void) {
    return &g.pattern;
}

const seq_model_gen_t *seq_led_bridge_get_generation(void) {
    return &g.pattern.generation;
}
