/**
 * @file seq_led_bridge.c
 * @brief Sequencer LED bridge relying on seq_model track snapshots.
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */

#include <stddef.h>
#include <string.h>

#include "apps/rtos_shim.h"
#include "brick_config.h"

#include "core/seq/reader/seq_reader.h"
#include "core/seq/runtime/seq_sections.h"
#include "core/seq/seq_access.h"
#include "seq_led_bridge.h"
#include "seq_engine_runner.h"
#include "seq_recorder.h"
#include "ui_mute_backend.h"
#include "ui_led_backend.h"
#include "core/ram_audit.h"
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_ids.h"
#endif

#ifdef BRICK_DEBUG_PLOCK
#include "chprintf.h"
#ifndef BRICK_DEBUG_PLOCK_STREAM
#define BRICK_DEBUG_PLOCK_STREAM ((BaseSequentialStream *)NULL)
#endif
#endif

#ifndef BRICK_DEBUG_PLOCK_LOG
#ifdef BRICK_DEBUG_PLOCK
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { \
        if (BRICK_DEBUG_PLOCK_STREAM != NULL) { \
            chprintf(BRICK_DEBUG_PLOCK_STREAM, "[PLOCK][%s] param=%u value=%ld t=%lu\r\n", \
                     (tag), (unsigned)(param), (long)(value), (unsigned long)(time)); \
        } \
    } while (0)
#else
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { (void)(tag); (void)(param); (void)(value); (void)(time); } while (0)
#endif
#endif

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
    seq_project_t        *project;      /**< Multi-track project handle. */
    seq_model_track_t  *track;        /**< Backing sequencer track reference. */
    uint16_t              page_hold_mask[SEQ_MAX_PAGES]; /**< Held-step mask per page (UI only). */
    uint16_t              preview_mask; /**< Cached mask for the visible page. */
    seq_led_runtime_t     rt;           /**< Runtime payload consumed by ui_led_seq. */
    uint8_t              max_pages;     /**< Number of usable pages. */
    uint8_t              visible_page;  /**< Currently focused page. */
    uint16_t             total_span;    /**< Track span exposed to LEDs (pages × 16). */
    uint8_t              last_note;     /**< Last armed note used for quick steps. */
    uint8_t              track_index;   /**< Currently bound track (for future multi-track). */
    uint8_t              track_count;   /**< Active track count (project view). */
    seq_led_bridge_hold_view_t hold;    /**< Aggregated hold/tweak snapshot. */
} seq_led_bridge_state_t;

static CCM_DATA seq_led_bridge_state_t g;
UI_RAM_AUDIT(g);

typedef struct {
    uint8_t hold_slots[SEQ_LED_BRIDGE_STEPS_PER_PAGE];
    uint8_t active_bank;
    uint8_t active_pattern;
} seq_led_bridge_cache_t;

static seq_led_bridge_cache_t g_cache;

#if SEQ_FEATURE_PLOCK_POOL
enum {
    k_seq_led_bridge_pl_flag_domain_cart = 0x01U,
    k_seq_led_bridge_pl_flag_signed = 0x02U,
    k_seq_led_bridge_pl_flag_voice_shift = 2U,
};

static int16_t _clamp_i16_local(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint8_t _encode_internal_plock_id(seq_model_plock_internal_param_t param, uint8_t voice) {
    switch (param) {
        case SEQ_MODEL_PLOCK_PARAM_NOTE:
            return (uint8_t)(PL_INT_NOTE_V0 + (voice & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_VELOCITY:
            return (uint8_t)(PL_INT_VEL_V0 + (voice & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_LENGTH:
            return (uint8_t)(PL_INT_LEN_V0 + (voice & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_MICRO:
            return (uint8_t)(PL_INT_MIC_V0 + (voice & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR:
            return PL_INT_ALL_TRANSP;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE:
            return PL_INT_ALL_VEL;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE:
            return PL_INT_ALL_LEN;
        case SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI:
            return PL_INT_ALL_MIC;
        default:
            break;
    }
    return 0U;
}

static uint8_t _encode_signed_value(int16_t value, uint8_t *flags) {
    if (flags != NULL) {
        *flags = (uint8_t)(*flags | k_seq_led_bridge_pl_flag_signed);
    }
    int16_t clamped = _clamp_i16_local(value, -128, 127);
    return pl_u8_from_s8((int8_t)clamped);
}

static uint8_t _encode_unsigned_value(int16_t value, int16_t min_value, int16_t max_value) {
    int16_t clamped = _clamp_i16_local(value, min_value, max_value);
    if (clamped < 0) {
        clamped = 0;
    }
    return (uint8_t)(clamped & 0x00FF);
}

static void _pack_plock_entry(const seq_model_plock_t *plock,
                              uint8_t *out_id,
                              uint8_t *out_value,
                              uint8_t *out_flags) {
    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flags = 0U;

    if (plock != NULL) {
        if (plock->domain == SEQ_MODEL_PLOCK_CART) {
            id = (uint8_t)(plock->parameter_id & 0x00FFU);
            value = _encode_unsigned_value(plock->value, 0, 255);
            flags = (uint8_t)(flags | k_seq_led_bridge_pl_flag_domain_cart);
        } else {
            id = _encode_internal_plock_id(plock->internal_param, plock->voice_index);
            switch (plock->internal_param) {
                case SEQ_MODEL_PLOCK_PARAM_NOTE:
                    value = _encode_unsigned_value(plock->value, 0, 127);
                    flags = (uint8_t)(flags |
                                      ((plock->voice_index & 0x03U) << k_seq_led_bridge_pl_flag_voice_shift));
                    break;
                case SEQ_MODEL_PLOCK_PARAM_VELOCITY:
                    value = _encode_unsigned_value(plock->value, 0, 127);
                    flags = (uint8_t)(flags |
                                      ((plock->voice_index & 0x03U) << k_seq_led_bridge_pl_flag_voice_shift));
                    break;
                case SEQ_MODEL_PLOCK_PARAM_LENGTH:
                    value = _encode_unsigned_value(plock->value, 0, 255);
                    flags = (uint8_t)(flags |
                                      ((plock->voice_index & 0x03U) << k_seq_led_bridge_pl_flag_voice_shift));
                    break;
                case SEQ_MODEL_PLOCK_PARAM_MICRO:
                    value = _encode_signed_value(plock->value, &flags);
                    flags = (uint8_t)(flags |
                                      ((plock->voice_index & 0x03U) << k_seq_led_bridge_pl_flag_voice_shift));
                    break;
                case SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR:
                case SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE:
                case SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE:
                case SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI:
                    value = _encode_signed_value(plock->value, &flags);
                    break;
                default:
                    value = _encode_unsigned_value(plock->value, 0, 255);
                    break;
            }
        }
    }

    if (out_id != NULL) {
        *out_id = id;
    }
    if (out_value != NULL) {
        *out_value = value;
    }
    if (out_flags != NULL) {
        *out_flags = flags;
    }
}

static void _seq_led_bridge_commit_plock_pool(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    const uint8_t count = step->plock_count;
    if (count == 0U) {
        (void)seq_model_step_set_plocks_pooled(step, NULL, NULL, NULL, 0U);
        return;
    }

    uint8_t ids[SEQ_MODEL_MAX_PLOCKS_PER_STEP];
    uint8_t values[SEQ_MODEL_MAX_PLOCKS_PER_STEP];
    uint8_t flags[SEQ_MODEL_MAX_PLOCKS_PER_STEP];

    for (uint8_t i = 0U; i < count; ++i) {
        _pack_plock_entry(&step->plocks[i], &ids[i], &values[i], &flags[i]);
    }

    (void)seq_model_step_set_plocks_pooled(step, ids, values, flags, count);
}
#endif

static void _cache_reset(void) {
    memset(&g_cache, 0, sizeof(g_cache));
}

static inline const seq_model_track_t *_seq_led_bridge_track_const(void);
static inline bool _valid_step_index(uint16_t absolute);

static void _cache_refresh_hold_slots(uint16_t base_step) {
    memset(g_cache.hold_slots, 0, sizeof(g_cache.hold_slots));

#if SEQ_USE_HANDLES
    seq_track_handle_t active =
        seq_reader_make_handle(g_cache.active_bank, g_cache.active_pattern, g.track_index);
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        const uint16_t absolute = base_step + (uint16_t)local;
        if (!_valid_step_index(absolute)) {
            break;
        }
        seq_step_view_t view;
        if (seq_reader_get_step(active, (uint8_t)absolute, &view)) {
            g_cache.hold_slots[local] = view.flags;
        }
    }
#else
    const seq_model_track_t *track = _seq_led_bridge_track_const();
    if (track == NULL) {
        return;
    }
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        const uint16_t absolute = base_step + (uint16_t)local;
        if (!_valid_step_index(absolute)) {
            break;
        }
        const seq_model_step_t *src = &track->steps[absolute];
        uint8_t flags = 0U;
        if (seq_model_step_has_playable_voice(src)) {
            flags |= SEQ_STEPF_HAS_VOICE;
        }
        if (seq_model_step_has_seq_plock(src)) {
            flags |= (SEQ_STEPF_HAS_SEQ_PLOCK | SEQ_STEPF_HAS_ANY_PLOCK);
        }
        if (seq_model_step_has_cart_plock(src)) {
            flags |= (SEQ_STEPF_HAS_CART_PLOCK | SEQ_STEPF_HAS_ANY_PLOCK);
        }
        if (seq_model_step_is_automation_only(src)) {
            flags |= SEQ_STEPF_AUTOMATION_ONLY;
        }
        g_cache.hold_slots[local] = flags;
    }
#endif
}

static inline seq_project_t *_seq_led_bridge_project(void) {
    return g.project;
}

static inline seq_model_track_t *_seq_led_bridge_track(void) {
    return g.track;
}

static inline const seq_model_track_t *_seq_led_bridge_track_const(void) {
    return g.track;
}

static inline void _seq_led_bridge_bump_generation(void) {
    seq_model_track_t *track = _seq_led_bridge_track();
    if (track != NULL) {
        seq_model_gen_bump(&track->generation);
    }
}

typedef struct {
    bool active;
    uint16_t absolute_index;
    seq_model_step_t staged;
    bool mutated;
} seq_led_bridge_hold_slot_t;

#if SEQ_EXPERIMENT_MOVE_ONE_BLOCK
#define SEQ_LED_BRIDGE_HOLD_SLOTS_SEC SEQ_COLD_SEC
#else
#define SEQ_LED_BRIDGE_HOLD_SLOTS_SEC
#endif

SEQ_LED_BRIDGE_HOLD_SLOTS_SEC CCM_DATA seq_led_bridge_hold_slot_t g_hold_slots[SEQ_LED_BRIDGE_STEPS_PER_PAGE];
UI_RAM_AUDIT(g_hold_slots);
const size_t g_hold_slots_size = sizeof(g_hold_slots);
#undef SEQ_LED_BRIDGE_HOLD_SLOTS_SEC

#ifndef SEQ_LED_BRIDGE_MAX_CART_PARAMS
#define SEQ_LED_BRIDGE_MAX_CART_PARAMS 32U
#endif

typedef struct {
    bool used;
    uint16_t parameter_id;
    seq_led_bridge_hold_param_t view;
    uint8_t match_count;
} seq_led_bridge_hold_cart_entry_t;

static CCM_DATA seq_led_bridge_hold_cart_entry_t g_hold_cart_params[SEQ_LED_BRIDGE_MAX_CART_PARAMS];
UI_RAM_AUDIT(g_hold_cart_params);
static uint8_t g_hold_cart_param_count;

/* ===== Helpers ============================================================ */
static inline uint16_t _page_base(uint8_t page) {
    return (uint16_t)page * SEQ_LED_BRIDGE_STEPS_PER_PAGE;
}

static inline uint16_t _clamp_total_span(uint16_t span) {
    if (span < SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        span = SEQ_LED_BRIDGE_STEPS_PER_PAGE;
    }
    if (span > SEQ_MODEL_STEPS_PER_TRACK) {
        span = SEQ_MODEL_STEPS_PER_TRACK;
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
    return (absolute < g.total_span) && (absolute < SEQ_MODEL_STEPS_PER_TRACK);
}

static const seq_led_bridge_hold_slot_t *_hold_slots_view(size_t *out_count) {
    if (out_count != NULL) {
        *out_count = SEQ_LED_BRIDGE_STEPS_PER_PAGE;
    }
    return g_hold_slots;
}

static inline seq_model_step_t *_step_from_page(uint8_t local_step) {
    seq_model_track_t *track = _seq_led_bridge_track();
    if (track == NULL) {
        return NULL;
    }
    uint16_t absolute = _page_base(g.visible_page) + (uint16_t)local_step;
    if (!_valid_step_index(absolute)) {
        return NULL;
    }
    return &track->steps[absolute];
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
    seq_model_step_recompute_flags(step);
}

static void _hold_slots_clear(void) {
    memset(g_hold_slots, 0, sizeof(g_hold_slots));
}

static void _hold_cart_reset(void) {
    memset(g_hold_cart_params, 0, sizeof(g_hold_cart_params));
    g_hold_cart_param_count = 0U;
}

static uint8_t _resolve_step_note(const seq_model_step_t *step, uint8_t voice, uint8_t fallback) {
    if (step == NULL) {
        return fallback;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plk = &step->plocks[i];
        if ((plk->domain == SEQ_MODEL_PLOCK_INTERNAL) &&
            (plk->internal_param == SEQ_MODEL_PLOCK_PARAM_NOTE) &&
            (plk->voice_index == voice)) {
            int32_t value = plk->value;
            if (value < 0) {
                value = 0;
            } else if (value > 127) {
                value = 127;
            }
            return (uint8_t)value;
        }
    }

    return fallback;
}

static bool _ensure_primary_voice_for_seq(seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    seq_model_voice_t voice = step->voices[0];
    bool mutated = false;

    uint8_t fallback = voice.note;
    if ((voice.state != SEQ_MODEL_VOICE_ENABLED) || (voice.velocity == 0U)) {
        fallback = (g.last_note <= 127U) ? g.last_note : 60U;
    }
    uint8_t desired_note = _resolve_step_note(step, 0U, fallback);
    if (voice.note != desired_note) {
        voice.note = desired_note;
        mutated = true;
    }

    if (voice.velocity == 0U) {
        voice.velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        mutated = true;
    }
    if (voice.length == 0U) {
        voice.length = 1U;
        mutated = true;
    }
    if (voice.state != SEQ_MODEL_VOICE_ENABLED) {
        voice.state = SEQ_MODEL_VOICE_ENABLED;
        mutated = true;
    }

    if (mutated) {
        step->voices[0] = voice;
    }
    return mutated;
}

static bool _hold_commit_slot(uint8_t local) {
    if (local >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return false;
    }
    seq_led_bridge_hold_slot_t *slot = &g_hold_slots[local];
    if (!slot->active) {
        return false;
    }

    bool mutated = slot->mutated;
    seq_model_track_t *track = _seq_led_bridge_track();
    if ((track != NULL) && mutated && _valid_step_index(slot->absolute_index)) {
#if SEQ_FEATURE_PLOCK_POOL
        _seq_led_bridge_commit_plock_pool(&slot->staged);
#endif
        track->steps[slot->absolute_index] = slot->staged;
        seq_model_step_recompute_flags(&track->steps[slot->absolute_index]);
        const seq_model_voice_t *voice =
            seq_model_step_get_voice(&track->steps[slot->absolute_index], 0U);
        if ((voice != NULL) &&
            (voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            g.last_note = voice->note;
        }
    }

    memset(slot, 0, sizeof(*slot));
    return mutated;
}

static seq_led_bridge_hold_slot_t *_hold_resolve_slot(uint8_t local, bool ensure) {
    if (local >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return NULL;
    }

    seq_led_bridge_hold_slot_t *slot = &g_hold_slots[local];
    const uint16_t absolute = _page_base(g.visible_page) + (uint16_t)local;

    if (slot->active) {
        if (slot->absolute_index == absolute) {
            return slot;
        }
        if (!ensure) {
            return NULL;
        }
        bool mutated = _hold_commit_slot(local);
        if (mutated) {
            seq_model_track_t *track = _seq_led_bridge_track();
            if (track != NULL) {
                seq_model_gen_bump(&track->generation);
            }
        }
        slot = &g_hold_slots[local];
    } else if (!ensure) {
        return NULL;
    }

    if (g.visible_page >= SEQ_MAX_PAGES) {
        return NULL;
    }
    if (!_valid_step_index(absolute)) {
        return NULL;
    }

    seq_model_track_t *track = _seq_led_bridge_track();
    if (track == NULL) {
        return NULL;
    }

    slot->active = true;
    slot->absolute_index = absolute;
    slot->staged = track->steps[absolute];
    slot->mutated = false;
    return slot;
}

static void _hold_sync_mask(uint16_t mask) {
    if (g.visible_page >= SEQ_MAX_PAGES) {
        return;
    }

    const uint16_t current = g.page_hold_mask[g.visible_page];
    bool mutated = false;

    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        const uint16_t bit = (uint16_t)(1U << local);
        const bool want = (mask & bit) != 0U;
        const bool had = (current & bit) != 0U;

        if (want) {
            (void)_hold_resolve_slot(local, true);
        } else if (had) {
            if (_hold_commit_slot(local)) {
                mutated = true;
            }
        } else {
            seq_led_bridge_hold_slot_t *slot = &g_hold_slots[local];
            if (slot->active && (slot->absolute_index != (_page_base(g.visible_page) + (uint16_t)local))) {
                if (_hold_commit_slot(local)) {
                    mutated = true;
                }
            }
        }
    }

    if (mutated) {
        seq_model_track_t *track = _seq_led_bridge_track();
        if (track != NULL) {
            seq_model_gen_bump(&track->generation);
        }
    }

    g.page_hold_mask[g.visible_page] = mask & 0xFFFFu;
}

static const seq_model_step_t *_hold_step_for_view(uint8_t local, uint16_t absolute) {
    if (!_valid_step_index(absolute)) {
        return NULL;
    }

    size_t slot_count = 0U;
    const seq_led_bridge_hold_slot_t *slots = _hold_slots_view(&slot_count);
    if ((slots == NULL) || (local >= slot_count)) {
        return NULL;
    }

    const seq_led_bridge_hold_slot_t *slot = &slots[local];
    if (slot->active && slot->absolute_index == absolute) {
        return &slot->staged;
    }
    const seq_model_track_t *track = _seq_led_bridge_track_const();
    if (track == NULL) {
        return NULL;
    }
    return &track->steps[absolute];
}

static seq_led_bridge_hold_cart_entry_t *_hold_cart_entry_get(uint16_t parameter_id,
                                                             bool create) {
    for (uint8_t i = 0U; i < g_hold_cart_param_count; ++i) {
        seq_led_bridge_hold_cart_entry_t *entry = &g_hold_cart_params[i];
        if (entry->used && entry->parameter_id == parameter_id) {
            return entry;
        }
    }

    if (!create || (g_hold_cart_param_count >= SEQ_LED_BRIDGE_MAX_CART_PARAMS)) {
        return NULL;
    }

    seq_led_bridge_hold_cart_entry_t *entry = &g_hold_cart_params[g_hold_cart_param_count++];
    entry->used = true;
    entry->parameter_id = parameter_id;
    entry->match_count = 0U;
    entry->view.available = false;
    entry->view.mixed = false;
    entry->view.plocked = false;
    entry->view.value = 0;
    return entry;
}

static void _hold_reset(void) {
    memset(&g.hold, 0, sizeof(g.hold));
    _hold_cart_reset();
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

static void _hold_collect_step(const seq_model_step_t *step,
                               bool present[SEQ_HOLD_PARAM_COUNT],
                               bool plocked[SEQ_HOLD_PARAM_COUNT],
                               int32_t values[SEQ_HOLD_PARAM_COUNT]) {
    if ((step == NULL) || (present == NULL) || (values == NULL)) {
        return;
    }

    /* Baseline offsets (All page). */
    present[SEQ_HOLD_PARAM_ALL_TRANSP] = true;
    values[SEQ_HOLD_PARAM_ALL_TRANSP] = step->offsets.transpose;
    present[SEQ_HOLD_PARAM_ALL_VEL] = true;
    values[SEQ_HOLD_PARAM_ALL_VEL] = step->offsets.velocity;
    present[SEQ_HOLD_PARAM_ALL_LEN] = true;
    values[SEQ_HOLD_PARAM_ALL_LEN] = step->offsets.length;
    present[SEQ_HOLD_PARAM_ALL_MIC] = true;
    values[SEQ_HOLD_PARAM_ALL_MIC] = step->offsets.micro;

    for (uint8_t voice = 0U; voice < SEQ_MODEL_VOICES_PER_STEP; ++voice) {
        const seq_model_voice_t *v = &step->voices[voice];
        const seq_hold_param_id_t base = SEQ_HOLD_PARAM_VOICE_BASE(voice);
        present[base + 0U] = true;
        values[base + 0U] = v->note;
        present[base + 1U] = true;
        values[base + 1U] = v->velocity;
        present[base + 2U] = true;
        values[base + 2U] = v->length;
        present[base + 3U] = true;
        values[base + 3U] = v->micro_offset;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plk = &step->plocks[i];
        if (plk->domain != SEQ_MODEL_PLOCK_INTERNAL) {
            continue;
        }
        if (plk->voice_index >= SEQ_MODEL_VOICES_PER_STEP) {
            continue;
        }
        const seq_hold_param_id_t pid =
            _hold_param_for_internal(plk->internal_param, plk->voice_index);
        if (pid >= SEQ_HOLD_PARAM_COUNT) {
            continue;
        }
        present[pid] = true;
        plocked[pid] = true;
        values[pid] = plk->value;
    }
}

static void _hold_collect_cart_plocks(const seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plk = &step->plocks[i];
        if (plk->domain != SEQ_MODEL_PLOCK_CART) {
            continue;
        }
        seq_led_bridge_hold_cart_entry_t *entry =
            _hold_cart_entry_get(plk->parameter_id, true);
        if (entry == NULL) {
            continue;
        }
        entry->match_count++;
        if (!entry->view.available) {
            entry->view.available = true;
            entry->view.value = plk->value;
            entry->view.mixed = false;
        } else if (entry->view.value != plk->value) {
            entry->view.mixed = true;
        }
    }
}

static void _hold_update(uint16_t mask) {
    _hold_reset();

    if ((mask == 0U) || (g.visible_page >= SEQ_MAX_PAGES)) {
        return;
    }

    g.hold.active = true;
    g.hold.mask = mask;

    bool first = true;
    bool available_all[SEQ_HOLD_PARAM_COUNT] = { false };
    bool plock_all[SEQ_HOLD_PARAM_COUNT] = { false };
    bool mixed[SEQ_HOLD_PARAM_COUNT] = { false };
    int32_t values[SEQ_HOLD_PARAM_COUNT] = { 0 };

    const uint16_t base = _page_base(g.visible_page);
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        if ((mask & (1U << local)) == 0U) {
            continue;
        }
        const uint16_t absolute = base + (uint16_t)local;
        if (!_valid_step_index(absolute)) {
            continue;
        }
        const seq_model_step_t *step = _hold_step_for_view(local, absolute);

        bool present[SEQ_HOLD_PARAM_COUNT] = { false };
        bool step_plocked[SEQ_HOLD_PARAM_COUNT] = { false };
        int32_t step_values[SEQ_HOLD_PARAM_COUNT] = { 0 };
        _hold_collect_step(step, present, step_plocked, step_values);
        _hold_collect_cart_plocks(step);

        if (first) {
            for (uint8_t i = 0U; i < SEQ_HOLD_PARAM_COUNT; ++i) {
                available_all[i] = present[i];
                if (present[i]) {
                    values[i] = step_values[i];
                    plock_all[i] = step_plocked[i];
                } else {
                    plock_all[i] = false;
                }
            }
            first = false;
        } else {
            for (uint8_t i = 0U; i < SEQ_HOLD_PARAM_COUNT; ++i) {
                if (!available_all[i]) {
                    continue;
                }
                if (!present[i]) {
                    available_all[i] = false;
                    plock_all[i] = false;
                    continue;
                }
                if (values[i] != step_values[i]) {
                    mixed[i] = true;
                }
                if (!step_plocked[i]) {
                    plock_all[i] = false;
                }
            }
        }

        ++g.hold.step_count;
    }

    if (g.hold.step_count == 0U) {
        _hold_reset();
        return;
    }

    for (uint8_t i = 0U; i < g_hold_cart_param_count; ++i) {
        seq_led_bridge_hold_cart_entry_t *entry = &g_hold_cart_params[i];
        if (!entry->used) {
            continue;
        }
        if (entry->match_count == g.hold.step_count) {
            entry->view.plocked = true;
        } else {
            entry->view.available = false;
            entry->view.plocked = false;
        }
    }

    for (uint8_t i = 0U; i < SEQ_HOLD_PARAM_COUNT; ++i) {
        g.hold.params[i].available = available_all[i];
        g.hold.params[i].mixed = mixed[i] && available_all[i];
        g.hold.params[i].plocked = available_all[i] && plock_all[i];
        if (available_all[i]) {
            g.hold.params[i].value = values[i];
        } else {
            g.hold.params[i].value = 0;
        }
    }
}

static void _hold_refresh_if_active(void) {
    if (g.hold.active) {
        _hold_update(g.preview_mask);
    }
}

static uint8_t _first_active_voice(const seq_model_step_t *step) {
    if (step == NULL) {
        return 0U;
    }

    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        const seq_model_voice_t *voice = &step->voices[v];
        if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            return v;
        }
    }

    return 0U;
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

    return seq_model_step_add_plock(step, &plock);
}

static bool _ensure_cart_plock_value(seq_model_step_t *step,
                                     uint16_t parameter_id,
                                     uint8_t track,
                                     int32_t value) {
    if (step == NULL) {
        return false;
    }

    const int16_t casted = (int16_t)value;
    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        seq_model_plock_t *plk = &step->plocks[i];
        if ((plk->domain == SEQ_MODEL_PLOCK_CART) && (plk->parameter_id == parameter_id)) {
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
        .domain = SEQ_MODEL_PLOCK_CART,
        .voice_index = track,
        .parameter_id = parameter_id,
        .value = casted,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };

    return seq_model_step_add_plock(step, &plock);
}

static void _update_preview_mask(void) {
    if (g.visible_page >= SEQ_MAX_PAGES) {
        g.preview_mask = 0U;
        return;
    }
    g.preview_mask = g.page_hold_mask[g.visible_page];
    g.rt.plock_selected_mask = g.preview_mask;
}

static void _rebuild_runtime_from_track(void) {
    memset(&g.rt, 0, sizeof(g.rt));
    g.rt.visible_page = g.visible_page;
    g.rt.steps_per_page = SEQ_LED_BRIDGE_STEPS_PER_PAGE;
    g.rt.plock_selected_mask = g.preview_mask;

    if (_seq_led_bridge_track_const() == NULL) {
        return;
    }
#if SEQ_USE_HANDLES
    const seq_track_handle_t handle = seq_reader_get_active_track_handle();
#else
    const seq_model_track_t *track = _seq_led_bridge_track_const();
#endif

    const uint16_t base = _page_base(g.visible_page);
    _cache_refresh_hold_slots(base);
    for (uint8_t local = 0U; local < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++local) {
        const uint16_t absolute = base + (uint16_t)local;
        seq_step_state_t *dst = &g.rt.steps[local];
        *dst = (seq_step_state_t){0};

        if (!_valid_step_index(absolute)) {
            continue;
        }

#if SEQ_USE_HANDLES
        seq_step_view_t view;
        if (!seq_reader_get_step(handle, (uint8_t)absolute, &view)) {
            continue;
        }
        const uint8_t step_flags = view.flags;
        const bool has_voice = (step_flags & SEQ_STEPF_HAS_VOICE) != 0U;
        const bool has_seq_plock = (step_flags & SEQ_STEPF_HAS_SEQ_PLOCK) != 0U;
        const bool has_cart_plock = (step_flags & SEQ_STEPF_HAS_CART_PLOCK) != 0U;
        const bool automation = (step_flags & SEQ_STEPF_AUTOMATION_ONLY) != 0U;
        dst->active = has_voice || has_seq_plock;
        dst->recorded = has_voice || has_seq_plock || has_cart_plock;
        dst->param_only = automation;
        dst->automation = automation;
        dst->muted = false;
#else
        const seq_model_step_t *src = &track->steps[absolute];
        const bool has_voice = seq_model_step_has_playable_voice(src);
        const bool has_seq_plock = seq_model_step_has_seq_plock(src);
        const bool has_cart_plock = seq_model_step_has_cart_plock(src);
        const bool automation = (!has_voice) && !has_seq_plock && has_cart_plock;
        const uint8_t voice = _first_active_voice(src);
        const bool muted = ui_mute_backend_is_muted(g.track_index);

        dst->active = has_voice || has_seq_plock;
        dst->recorded = has_voice || has_seq_plock || has_cart_plock;
        dst->param_only = automation;
        dst->automation = automation;
        (void)muted; /* Track mute reflété uniquement en mode MUTE (pas en SEQ). */
        dst->muted = false;
        dst->track = voice;
#endif
    }

    ui_led_seq_set_total_span(g.total_span);
    ui_led_seq_update_from_app(&g.rt);
}

static void _publish_runtime(void) {
    seq_project_t *project = _seq_led_bridge_project();
    if (project != NULL) {
        g.track_index = seq_project_get_active_track_index(project);
        g.track_count = seq_project_get_track_count(project);
        g.track = seq_project_get_active_track(project);
    } else {
        g.track_index = 0U;
        g.track_count = 0U;
        g.track = NULL;
        _cache_reset();
    }

    g.visible_page = _clamp_page(g.visible_page);

    _update_preview_mask();
    _rebuild_runtime_from_track();
    _hold_refresh_if_active();
    {
        uint8_t per_slot[4] = {0, 0, 0, 0};
        for (uint8_t track = 0U; track < SEQ_PROJECT_MAX_TRACKS; ++track) {
            bool present = false;
            if (project != NULL) {
                const seq_model_track_t *tp =
                    seq_project_get_track_const(project, track);
                present = (tp != NULL);
            }
            ui_led_backend_set_track_present(track, present);
            const uint8_t slot = (uint8_t)(track / 4U);
            const uint8_t pos  = (uint8_t)(track % 4U);
            if (present && slot < 4U) {
                uint8_t span = (uint8_t)(pos + 1U);
                if (per_slot[slot] < span) {
                    per_slot[slot] = span;
                }
            }
        }
        for (uint8_t slot = 0U; slot < 4U; ++slot) {
            ui_led_backend_set_cart_track_count(slot, per_slot[slot]);
        }
        ui_led_backend_set_track_focus(g.track_index);
    }
}

/* ===== API =============================================================== */
void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern) {
    g_cache.active_bank = bank;
    g_cache.active_pattern = pattern;
    memset(g_cache.hold_slots, 0, sizeof(g_cache.hold_slots));
}

void seq_led_bridge_get_active(uint8_t *out_bank, uint8_t *out_pattern) {
    if (out_bank != NULL) {
        *out_bank = g_cache.active_bank;
    }
    if (out_pattern != NULL) {
        *out_pattern = g_cache.active_pattern;
    }
}

void seq_led_bridge_init(void) {
    memset(&g, 0, sizeof(g));
    _cache_reset();
    g.project = NULL;
    g.track = NULL;
    g.track_index = 0U;
    g.track_count = 0U;
    _hold_slots_clear();
    _hold_cart_reset();
    g.last_note = 60U;
    g.max_pages = (SEQ_DEFAULT_PAGES > SEQ_MAX_PAGES) ? SEQ_MAX_PAGES : SEQ_DEFAULT_PAGES;
    g.total_span = _clamp_total_span((uint16_t)g.max_pages * SEQ_LED_BRIDGE_STEPS_PER_PAGE);
    if (g.max_pages == 0U) {
        g.max_pages = 1U;
    }
    g.visible_page = 0U;

    _publish_runtime();
}

void seq_led_bridge_bind_project(seq_project_t *project) {
    seq_project_t *previous = g.project;
    g.project = project;
    if (project == NULL) {
        g.track = NULL;
        g.track_index = 0U;
        g.track_count = 0U;
        _hold_slots_clear();
        _hold_cart_reset();
        g.hold.active = false;
        g.hold.mask = 0U;
        memset(g.hold.params, 0, sizeof(g.hold.params));
        memset(g.page_hold_mask, 0, sizeof(g.page_hold_mask));
        g.preview_mask = 0U;
    } else if (project != previous) {
        _hold_slots_clear();
        _hold_cart_reset();
        g.hold.active = false;
        g.hold.mask = 0U;
        memset(g.hold.params, 0, sizeof(g.hold.params));
        memset(g.page_hold_mask, 0, sizeof(g.page_hold_mask));
        g.preview_mask = 0U;
    }
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
    _seq_led_bridge_bump_generation();
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
    seq_model_step_recompute_flags(step);
    if (seq_model_step_has_playable_voice(step) &&
        (voice_idx == 0U) &&
        (voice.state == SEQ_MODEL_VOICE_ENABLED) && (voice.velocity > 0U)) {
        g.last_note = voice.note;
    }
    _seq_led_bridge_bump_generation();
    _hold_refresh_if_active();
}

void seq_led_bridge_step_set_has_plock(uint8_t i, bool on) {
    seq_led_bridge_hold_slot_t *slot = _hold_resolve_slot(i, false);
    seq_model_step_t *step = (slot != NULL) ? &slot->staged : _step_from_page(i);
    if (step == NULL) {
        return;
    }

    bool mutated = false;
    if (on) {
        const bool has_voice = seq_model_step_has_playable_voice(step);
        const bool has_seq_plock = seq_model_step_has_seq_plock(step);
        if (!has_voice && !has_seq_plock) {
            seq_model_step_make_automation_only(step);
            mutated = true;
        }
    } else if (step->plock_count > 0U) {
        seq_model_step_clear_plocks(step);
        mutated = true;
    }

    if (mutated) {
        seq_model_step_recompute_flags(step);
        if (slot != NULL) {
            slot->mutated = true;
        } else {
            _seq_led_bridge_bump_generation();
        }
    }
    _hold_refresh_if_active();
}

void seq_led_bridge_quick_toggle_step(uint8_t i) {
    seq_model_step_t *step = _step_from_page(i);
    if (step == NULL) {
        return;
    }

    const bool was_on = seq_model_step_has_playable_voice(step) ||
                        seq_model_step_is_automation_only(step) ||
                        seq_model_step_has_any_plock(step);
    if (was_on) {
        seq_led_bridge_step_clear(i);
    } else {
        seq_model_step_init_default(step, g.last_note);
        const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);
        if (voice != NULL) {
            g.last_note = voice->note;
        }
        _seq_led_bridge_bump_generation();
        _hold_refresh_if_active();
        // --- FIX: suppression du step preview MIDI pour éviter les notes parasites ---
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_set_step_param_only(uint8_t i, bool on) {
    seq_led_bridge_hold_slot_t *slot = _hold_resolve_slot(i, false);
    seq_model_step_t *step = (slot != NULL) ? &slot->staged : _step_from_page(i);
    if (step == NULL) {
        return;
    }

    bool mutated = false;
    if (on) {
        const bool has_voice = seq_model_step_has_playable_voice(step);
        const bool has_seq_plock = seq_model_step_has_seq_plock(step);
        if (!has_voice && !has_seq_plock) {
            seq_model_step_make_automation_only(step);
            mutated = true;
        }
    } else if (step->plock_count > 0U) {
        seq_model_step_clear_plocks(step);
        mutated = true;
    }

    if (mutated) {
        seq_model_step_recompute_flags(step);
        if (slot != NULL) {
            slot->mutated = true;
        } else {
            _seq_led_bridge_bump_generation();
        }
    }
    _hold_refresh_if_active();
    seq_led_bridge_publish();
}

void seq_led_bridge_on_play(void) {
    ui_led_seq_set_running(true);
}

void seq_led_bridge_on_stop(void) {
    ui_led_seq_set_running(false);
    _hold_sync_mask(0U);
    g.preview_mask = 0U;
    _hold_update(0U);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_add(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    uint16_t mask = g.page_hold_mask[g.visible_page] | (uint16_t)(1U << i);
    _hold_sync_mask(mask);
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_remove(uint8_t i) {
    if (g.visible_page >= SEQ_MAX_PAGES || i >= SEQ_LED_BRIDGE_STEPS_PER_PAGE) {
        return;
    }
    uint16_t mask = g.page_hold_mask[g.visible_page] & (uint16_t)~(1U << i);
    _hold_sync_mask(mask);
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_begin_plock_preview(uint16_t held_mask) {
    BRICK_DEBUG_PLOCK_LOG("UI_HOLD_START", held_mask, 0, chVTGetSystemTimeX());
    _hold_sync_mask(held_mask & 0xFFFFu);
    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_apply_plock_param(seq_hold_param_id_t param_id,
                                      int32_t value,
                                      uint16_t held_mask) {
    BRICK_DEBUG_PLOCK_LOG("UI_ENCODER", param_id, value, chVTGetSystemTimeX());
    if ((g.visible_page >= SEQ_MAX_PAGES) || (param_id >= SEQ_HOLD_PARAM_COUNT)) {
        return;
    }

    bool mutated_track = false;

    for (uint8_t i = 0U; i < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++i) {
        if ((held_mask & (1U << i)) == 0U) {
            continue;
        }
        seq_led_bridge_hold_slot_t *slot = _hold_resolve_slot(i, true);
        seq_model_step_t *step = (slot != NULL) ? &slot->staged : _step_from_page(i);
        if (step == NULL) {
            continue;
        }

        const bool had_voice = seq_model_step_has_playable_voice(step);
        const bool had_plock = seq_model_step_has_any_plock(step);
        bool step_mutated = false;
        if (!had_voice) {
            if (!had_plock) {
                seq_model_step_make_neutral(step);
                step_mutated = true;
            } else if (_ensure_primary_voice_for_seq(step)) {
                step_mutated = true;
            }
        }

        switch (param_id) {
            case SEQ_HOLD_PARAM_ALL_TRANSP: {
                int32_t v = _clamp_i32(value, -12, 12);
                if (step->offsets.transpose != v) {
                    step->offsets.transpose = (int8_t)v;
                    step_mutated = true;
                }
                step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_VEL: {
                int32_t v = _clamp_i32(value, -127, 127);
                if (step->offsets.velocity != v) {
                    step->offsets.velocity = (int16_t)v;
                    step_mutated = true;
                }
                step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_LEN: {
                int32_t v = _clamp_i32(value, -32, 32);
                if (step->offsets.length != v) {
                    step->offsets.length = (int8_t)v;
                    step_mutated = true;
                }
                step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE, 0U, v);
                break;
            }
            case SEQ_HOLD_PARAM_ALL_MIC: {
                int32_t v = _clamp_i32(value, -12, 12);
                if (step->offsets.micro != v) {
                    step->offsets.micro = (int8_t)v;
                    step_mutated = true;
                }
                step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI, 0U, v);
                break;
            }
            default: {
                const seq_hold_param_id_t base = SEQ_HOLD_PARAM_V1_NOTE;
                if (param_id < base) {
                    break;
                }
                int32_t rel = (int32_t)param_id - (int32_t)base;
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
                            step_mutated = true;
                        }
                        step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_NOTE, voice, v);
                        if ((voice == 0U) &&
                            (voice_state.state == SEQ_MODEL_VOICE_ENABLED) &&
                            (voice_state.velocity > 0U)) {
                            g.last_note = voice_state.note;
                        }
                        break;
                    }
                    case 1: { /* Velocity */
                        int32_t v = _clamp_i32(value, 0, 127);
                        uint8_t applied_velocity = (uint8_t)v;
                        if (voice_state.velocity != applied_velocity) {
                            voice_state.velocity = applied_velocity;
                            step_mutated = true;
                        }
                        voice_state.state = (voice_state.velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
                        step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_VELOCITY, voice, v);
                        break;
                    }
                    case 2: { /* Length */
                        int32_t v = _clamp_i32(value, 1, 64);
                        if (voice_state.length != (uint8_t)v) {
                            voice_state.length = (uint8_t)v;
                            step_mutated = true;
                        }
                        step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_LENGTH, voice, v);
                        break;
                    }
                    case 3: { /* Micro */
                        int32_t v = _clamp_i32(value, -12, 12);
                        if (voice_state.micro_offset != (int8_t)v) {
                            voice_state.micro_offset = (int8_t)v;
                            step_mutated = true;
                        }
                        step_mutated |= _ensure_internal_plock_value(step, SEQ_MODEL_PLOCK_PARAM_MICRO, voice, v);
                        break;
                    }
                    default:
                        break;
                }
                step->voices[voice] = voice_state;
                break;
            }
        }

        seq_model_step_recompute_flags(step);
#if SEQ_FEATURE_PLOCK_POOL
        if (step_mutated && (slot == NULL)) {
            _seq_led_bridge_commit_plock_pool(step);
        }
#endif
        if (step_mutated) {
            if (slot != NULL) {
                slot->mutated = true;
            } else {
                mutated_track = true;
            }
        }
    }

    if (mutated_track) {
        _seq_led_bridge_bump_generation();
    }

    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

void seq_led_bridge_end_plock_preview(void) {
    BRICK_DEBUG_PLOCK_LOG("UI_HOLD_COMMIT", g.hold.mask, 0, chVTGetSystemTimeX());
    _hold_sync_mask(0U);
    _hold_update(0U);
    seq_led_bridge_publish();
}

const seq_led_bridge_hold_view_t *seq_led_bridge_get_hold_view(void) {
    return &g.hold;
}

bool seq_led_bridge_hold_get_cart_param(uint16_t parameter_id,
                                        seq_led_bridge_hold_param_t *out) {
    if (out == NULL) {
        return false;
    }

    for (uint8_t i = 0U; i < g_hold_cart_param_count; ++i) {
        const seq_led_bridge_hold_cart_entry_t *entry = &g_hold_cart_params[i];
        if (entry->used && entry->parameter_id == parameter_id) {
            *out = entry->view;
            return true;
        }
    }

    memset(out, 0, sizeof(*out));
    return false;
}

void seq_led_bridge_apply_cart_param(uint16_t parameter_id,
                                     int32_t value,
                                     uint16_t held_mask) {
    BRICK_DEBUG_PLOCK_LOG("UI_ENCODER_CART", parameter_id, value, chVTGetSystemTimeX());
    if (g.visible_page >= SEQ_MAX_PAGES) {
        return;
    }

    bool mutated_track = false;

    for (uint8_t i = 0U; i < SEQ_LED_BRIDGE_STEPS_PER_PAGE; ++i) {
        if ((held_mask & (1U << i)) == 0U) {
            continue;
        }

        seq_led_bridge_hold_slot_t *slot = _hold_resolve_slot(i, true);
        seq_model_step_t *step = (slot != NULL) ? &slot->staged : _step_from_page(i);
        if (step == NULL) {
            continue;
        }

        const uint8_t track = _first_active_voice(step);
        const bool had_voice = seq_model_step_has_playable_voice(step);
        const bool had_plock = seq_model_step_has_any_plock(step);
        if (!had_voice && !had_plock) {
            seq_model_step_make_automation_only(step);
            if (slot != NULL) {
                slot->mutated = true;
            } else {
                mutated_track = true;
            }
        }

        const bool cart_mutated = _ensure_cart_plock_value(step, parameter_id, track, value);
        if (cart_mutated) {
            if (slot != NULL) {
                slot->mutated = true;
            } else {
                mutated_track = true;
            }
        }

        seq_model_step_recompute_flags(step);
#if SEQ_FEATURE_PLOCK_POOL
        if ((slot == NULL) && ((!had_voice && !had_plock) || cart_mutated)) {
            _seq_led_bridge_commit_plock_pool(step);
        }
#endif
    }

    if (mutated_track) {
        _seq_led_bridge_bump_generation();
    }

    _hold_update(g.page_hold_mask[g.visible_page]);
    seq_led_bridge_publish();
}

seq_model_track_t *seq_led_bridge_access_track(void) {
    return _seq_led_bridge_track();
}

const seq_model_track_t *seq_led_bridge_get_track(void) {
    return _seq_led_bridge_track_const();
}

const seq_model_gen_t *seq_led_bridge_get_generation(void) {
    const seq_model_track_t *track = _seq_led_bridge_track_const();
    return (track != NULL) ? &track->generation : NULL;
}

seq_project_t *seq_led_bridge_get_project(void) {
    return g.project;
}

const seq_project_t *seq_led_bridge_get_project_const(void) {
    return g.project;
}

uint8_t seq_led_bridge_get_track_index(void) {
    return g.track_index;
}

uint8_t seq_led_bridge_get_track_count(void) {
    return g.track_count;
}

bool seq_led_bridge_select_track(uint8_t track_index) {
    seq_project_t *project = _seq_led_bridge_project();
    if ((project == NULL) || !seq_project_set_active_track(project, track_index)) {
        return false;
    }

    g.track_index = seq_project_get_active_track_index(project);
    g.track_count = seq_project_get_track_count(project);
    g.track = seq_project_get_active_track(project);

    _hold_slots_clear();
    _hold_cart_reset();
    g.hold.active = false;
    g.hold.mask = 0U;
    memset(g.hold.params, 0, sizeof(g.hold.params));
    memset(g.page_hold_mask, 0, sizeof(g.page_hold_mask));
    g.preview_mask = 0U;

    seq_model_track_t *track_model = _seq_led_bridge_track();
    if (track_model != NULL) {
        seq_recorder_attach_track(track_model);
    }

    _publish_runtime();
    return true;
}
