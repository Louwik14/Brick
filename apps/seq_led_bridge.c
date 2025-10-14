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
    seq_led_bridge_hold_view_t hold;   /**< Aggregated hold/tweak snapshot. */
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

static void _hold_reset(void) {
    memset(&g.hold, 0, sizeof(g.hold));
}

static seq_hold_param_id_t _hold_param_for_voice(uint8_t voice, uint8_t slot) {
    if (voice >= SEQ_MODEL_VOICES_PER_STEP || slot >= 4U) {
        return SEQ_HOLD_PARAM_COUNT;
    }
    return (seq_hold_param_id_t)(SEQ_HOLD_PARAM_V1_NOTE + (voice * 4U) + slot);
}

static seq_hold_param_id_t _hold_param_for_internal(seq_model_plock_internal_param_t internal,
                                                    uint8_t voice) {
    switch (internal) {
        case SEQ_MODEL_PLOCK_PARAM_NOTE:
            return _hold_param_for_voice(voice, 0U);
        case SEQ_MODEL_PLOCK_PARAM_VELOCITY:
            return _hold_param_for_voice(voice, 1U);
        case SEQ_MODEL_PLOCK_PARAM_LENGTH:
            return _hold_param_for_voice(voice, 2U);
        case SEQ_MODEL_PLOCK_PARAM_MICRO:
            return _hold_param_for_voice(voice, 3U);
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR:
            return SEQ_HOLD_PARAM_ALL_TRANSP;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE:
            return SEQ_HOLD_PARAM_ALL_VEL;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE:
            return SEQ_HOLD_PARAM_ALL_LEN;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI:
            return SEQ_HOLD_PARAM_ALL_MIC;
        default:
            return SEQ_HOLD_PARAM_COUNT;
    }
}

static void _hold_merge_param(seq_led_bridge_hold_param_t *param, int32_t value) {
    if (param == NULL) {
        return;
    }
    if (!param->available) {
        param->available = true;
        param->value = value;
    } else if (param->value != value) {
        param->mixed = true;
    }
}

static void _hold_accumulate_step(const seq_model_step_t *step, uint8_t absolute_index) {
    if (step == NULL) {
        return;
    }

    if ((absolute_index < SEQ_MODEL_STEPS_PER_PATTERN) &&
        (g.hold.step_count < SEQ_MODEL_STEPS_PER_PATTERN)) {
        g.hold.step_indexes[g.hold.step_count] = absolute_index;
    }
    g.hold.step_count++;

    const bool has_voice = seq_model_step_has_active_voice(step);
    if (!has_voice && (step->plock_count > 0U)) {
        g.hold.has_param_only = true;
    }
    if (step->plock_count > 0U) {
        g.hold.has_plock_preview = true;
    }

    _hold_merge_param(&g.hold.params[SEQ_HOLD_PARAM_ALL_TRANSP], step->offsets.transpose);
    _hold_merge_param(&g.hold.params[SEQ_HOLD_PARAM_ALL_VEL], step->offsets.velocity);
    _hold_merge_param(&g.hold.params[SEQ_HOLD_PARAM_ALL_LEN], step->offsets.length);
    _hold_merge_param(&g.hold.params[SEQ_HOLD_PARAM_ALL_MIC], step->offsets.micro);

    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        const seq_model_voice_t *voice = &step->voices[v];
        _hold_merge_param(&g.hold.params[_hold_param_for_voice(v, 0U)], voice->note);
        _hold_merge_param(&g.hold.params[_hold_param_for_voice(v, 1U)], voice->velocity);
        _hold_merge_param(&g.hold.params[_hold_param_for_voice(v, 2U)], voice->length);
        _hold_merge_param(&g.hold.params[_hold_param_for_voice(v, 3U)], voice->micro_offset);
    }

    for (uint8_t p = 0U; p < step->plock_count; ++p) {
        const seq_model_plock_t *plk = &step->plocks[p];
        if (plk->domain != SEQ_MODEL_PLOCK_INTERNAL) {
            continue;
        }
        const seq_hold_param_id_t pid =
            _hold_param_for_internal(plk->internal_param, plk->voice_index);
        if (pid < SEQ_HOLD_PARAM_COUNT) {
            g.hold.params[pid].plocked = true;
        }
    }
}

static void _hold_update(uint16_t mask) {
    _hold_reset();

    if ((mask == 0U) || (g.visible_page >= SEQ_MAX_PAGES)) {
        return;
    }

    g.hold.active = true;

    const uint16_t base = _page_base(g.visible_page);
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        if ((mask & (1U << local)) == 0U) {
            continue;
        }
        const uint16_t absolute = base + (uint16_t)local;
        if (!_valid_step_index(absolute)) {
            continue;
        }
        _hold_accumulate_step(&g.pattern.steps[absolute], (uint8_t)absolute);
    }

    if (g.hold.step_count == 0U) {
        _hold_reset();
    }
}

static void _hold_refresh_if_active(void) {
    if (g.hold.active) {
        _hold_update(g.preview_mask);
    }
}

static int32_t _clamp_i32(int32_t value, int32_t min, int32_t max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static bool _ensure_internal_plock_value(seq_model_step_t *step,
                                         seq_model_plock_internal_param_t param,
                                         uint8_t voice,
                                         int32_t value) {
    if (step == NULL) {
        return false;
    }
    const int16_t casted = (int16_t)value;
    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        seq_model_plock_t *plk = &step->plocks[i];
        if ((plk->domain == SEQ_MODEL_PLOCK_INTERNAL) &&
            (plk->internal_param == param) &&
            (plk->voice_index == voice)) {
            if (plk->value != casted) {
                plk->value = casted;
                return true;
            }
            return false;
        }
    }

    if (step->plock_count >= SEQ_MODEL_MAX_PLOCKS_PER_STEP) {
        return false;
    }

    seq_model_plock_t plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = voice,
        .parameter_id = 0U,
        .value = casted,
        .internal_param = param
    };

    if (seq_model_step_add_plock(step, &plock)) {
        return true;
    }
    return false;
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
    _hold_refresh_if_active();
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
    _hold_refresh_if_active();
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
    _hold_refresh_if_active();
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
    _hold_refresh_if_active();
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
        _hold_refresh_if_active();
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
    _hold_refresh_if_active();
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
    _hold_update(0U);
    seq_led_bridge_publish();
}

void seq_led_bridge_set_plock_mask(uint16_t mask) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = mask;
    }
    _hold_update(mask);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_add(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    g.page_hold_mask[g.visible_page] |= (1u << i);
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_remove(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    g.page_hold_mask[g.visible_page] &= (uint16_t)~(1u << i);
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_clear(void) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = 0U;
    }
    _hold_update(0U);
    seq_led_bridge_publish();
}

void seq_led_bridge_begin_plock_preview(uint16_t held_mask) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = held_mask & 0xFFFFu;
    }
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_apply_plock_param(seq_hold_param_id_t param_id,
                                      int32_t value,
                                      uint16_t held_mask) {
    if ((g.visible_page >= SEQ_MAX_PAGES) || (param_id >= SEQ_HOLD_PARAM_COUNT)) {
        return;
    }

    bool mutated = false;

    for (uint8_t i = 0U; i < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++i) {
        if ((held_mask & (1U << i)) == 0U) {
            continue;
        }
        seq_model_step_t *step = _step_from_page(i);
        if (step == NULL) {
            continue;
        }

        switch (param_id) {
            case SEQ_HOLD_PARAM_ALL_TRANSP: {
                int32_t v = _clamp_i32(value, -12, 12);
                if (step->offsets.transpose != v) {
                    step->offsets.transpose = (int8_t)v;
                    mutated = true;
                }
                mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_VEL: {
                int32_t v = _clamp_i32(value, -127, 127);
                if (step->offsets.velocity != v) {
                    step->offsets.velocity = (int16_t)v;
                    mutated = true;
                }
                mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_LEN: {
                int32_t v = _clamp_i32(value, -32, 32);
                if (step->offsets.length != v) {
                    step->offsets.length = (int8_t)v;
                    mutated = true;
                }
                mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_MIC: {
                int32_t v = _clamp_i32(value, -12, 12);
                if (step->offsets.micro != v) {
                    step->offsets.micro = (int8_t)v;
                    mutated = true;
                }
                mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI, 0U, v);
                break;
            }
            default: {
                const int32_t base = (int32_t)SEQ_HOLD_PARAM_V1_NOTE;
                if (param_id < base) {
                    break;
                }
                int32_t rel = (int32_t)param_id - base;
                uint8_t voice = (uint8_t)(rel / 4);
                uint8_t slot = (uint8_t)(rel % 4);
                if (voice >= SEQ_MODEL_VOICES_PER_STEP) {
                    break;
                }
                seq_model_voice_t voice_state = step->voices[voice];
                switch (slot) {
                    case 0: { /* Note */
                        int32_t v = _clamp_i32(value, 0, 127);
                        if (voice_state.note != (uint8_t)v) {
                            voice_state.note = (uint8_t)v;
                            mutated = true;
                        }
                        mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_NOTE, voice, v);
                        if ((voice == 0U) && (voice_state.state == SEQ_MODEL_VOICE_ENABLED) && (voice_state.velocity > 0U)) {
                            g.last_note = voice_state.note;
                        }
                        break;
                    }
                    case 1: { /* Velocity */
                        int32_t v = _clamp_i32(value, 0, 127);
                        if (voice_state.velocity != (uint8_t)v) {
                            voice_state.velocity = (uint8_t)v;
                            mutated = true;
                        }
                        voice_state.state = (voice_state.velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
                        mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_VELOCITY, voice, v);
                        break;
                    }
                    case 2: { /* Length */
                        int32_t v = _clamp_i32(value, 1, 64);
                        if (voice_state.length != (uint8_t)v) {
                            voice_state.length = (uint8_t)v;
                            mutated = true;
                        }
                        mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_LENGTH, voice, v);
                        break;
                    }
                    case 3: { /* Micro */
                        int32_t v = _clamp_i32(value, -12, 12);
                        if (voice_state.micro_offset != (int8_t)v) {
                            voice_state.micro_offset = (int8_t)v;
                            mutated = true;
                        }
                        mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_MICRO, voice, v);
                        break;
                    }
                    default:
                        break;
                }
                step->voices[voice] = voice_state;
                break;
            }
        }
    }

    if (mutated) {
        seq_model_gen_bump(&g.pattern.generation);
    }

    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_end_plock_preview(void) {
    if (g.visible_page < SEQ_MAX_PAGES) {
        g.page_hold_mask[g.visible_page] = 0U;
    }
    _hold_update(0U);
    seq_led_bridge_publish();
}

const seq_led_bridge_hold_view_t *seq_led_bridge_get_hold_view(void) {
    return &g.hold;
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
