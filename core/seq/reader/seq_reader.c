#include "core/seq/seq_config.h"       // IMPORTANT: en premier
#include "core/seq/reader/seq_reader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/runtime/seq_runtime_cold.h"
#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_runtime.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
typedef seq_step_plock_ref_t pl_ref_t;
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_pool.h"
#pragma GCC poison plocks
#pragma GCC poison plock_count
_Static_assert(sizeof(pl_ref_t) == 3, "pl_ref_t must be packed to 3 bytes");
_Static_assert(SEQ_MAX_PLOCKS_PER_STEP <= 24, "cap exceeded");
#else
#error "seq_reader hot requires SEQ_FEATURE_PLOCK_POOL"
#endif

enum {
    k_seq_reader_plock_internal_flag = 0x8000U,
    k_seq_reader_plock_internal_voice_shift = 8U,
};

typedef struct {
    uint16_t base;
    uint8_t count;
    uint8_t i;
} seq_reader_plock_iter_state_t;

typedef struct {
    uint8_t param_id;
    uint8_t value;
    uint8_t flags;
} seq_reader_plock_item_t;

static seq_reader_plock_iter_state_t s_plock_iter_state;
static seq_model_plock_internal_param_t _pool_internal_param_from_id(uint8_t id) {
    if ((id >= PL_INT_NOTE_V0) && (id <= PL_INT_NOTE_V3)) {
        return SEQ_MODEL_PLOCK_PARAM_NOTE;
    }
    if ((id >= PL_INT_VEL_V0) && (id <= PL_INT_VEL_V3)) {
        return SEQ_MODEL_PLOCK_PARAM_VELOCITY;
    }
    if ((id >= PL_INT_LEN_V0) && (id <= PL_INT_LEN_V3)) {
        return SEQ_MODEL_PLOCK_PARAM_LENGTH;
    }
    if ((id >= PL_INT_MIC_V0) && (id <= PL_INT_MIC_V3)) {
        return SEQ_MODEL_PLOCK_PARAM_MICRO;
    }

    switch (id) {
        case PL_INT_ALL_TRANSP:
            return SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR;
        case PL_INT_ALL_VEL:
            return SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE;
        case PL_INT_ALL_LEN:
            return SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE;
        case PL_INT_ALL_MIC:
            return SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI;
        default:
            break;
    }

    return SEQ_MODEL_PLOCK_PARAM_NOTE;
}

static uint8_t _pool_internal_voice_from_id(uint8_t id, uint8_t flags) {
    if ((id >= PL_INT_NOTE_V0) && (id <= PL_INT_NOTE_V3)) {
        return (uint8_t)(id - PL_INT_NOTE_V0);
    }
    if ((id >= PL_INT_VEL_V0) && (id <= PL_INT_VEL_V3)) {
        return (uint8_t)(id - PL_INT_VEL_V0);
    }
    if ((id >= PL_INT_LEN_V0) && (id <= PL_INT_LEN_V3)) {
        return (uint8_t)(id - PL_INT_LEN_V0);
    }
    if ((id >= PL_INT_MIC_V0) && (id <= PL_INT_MIC_V3)) {
        return (uint8_t)(id - PL_INT_MIC_V0);
    }

    return (uint8_t)((flags & SEQ_READER_PL_FLAG_VOICE_MASK) >> SEQ_READER_PL_FLAG_VOICE_SHIFT);
}

static uint16_t _pool_encode_plock_id(uint8_t param_id, uint8_t flags) {
    if (pl_is_cart(param_id) || ((flags & SEQ_READER_PL_FLAG_DOMAIN_CART) != 0U)) {
        return (uint16_t)param_id;
    }

    const uint8_t voice = _pool_internal_voice_from_id(param_id, flags) & 0x03U;
    const seq_model_plock_internal_param_t param = _pool_internal_param_from_id(param_id);
    const uint16_t voice_bits = ((uint16_t)voice) << k_seq_reader_plock_internal_voice_shift;
    return (uint16_t)(k_seq_reader_plock_internal_flag | voice_bits | (uint16_t)param);
}

static int32_t _pool_decode_plock_value(uint8_t value, uint8_t flags) {
    if ((flags & SEQ_READER_PL_FLAG_SIGNED) != 0U) {
        return (int32_t)pl_s8_from_u8(value);
    }
    return (int32_t)value;
}

static inline void reader_pack_from_pool(const seq_plock_entry_t *entry,
                                         seq_reader_plock_item_t *out) {
    if ((entry == NULL) || (out == NULL)) {
        return;
    }

    out->param_id = entry->param_id;
    out->value = entry->value;
    out->flags = entry->flags;
}

static const seq_model_track_t *_resolve_legacy_track(seq_track_handle_t handle) {
    if ((handle.bank >= SEQ_PROJECT_BANK_COUNT) ||
        (handle.pattern >= SEQ_PROJECT_PATTERNS_PER_BANK) ||
        (handle.track >= SEQ_PROJECT_MAX_TRACKS)) {
        return NULL;
    }

    const seq_runtime_blocks_t *blocks = seq_runtime_blocks_get();
    if ((blocks == NULL) || (blocks->hot_impl == NULL)) {
        return NULL;
    }

    const seq_project_t *project = (const seq_project_t *)blocks->hot_impl;
    if (project == NULL) {
        return NULL;
    }

    if ((project->active_bank != handle.bank) || (project->active_pattern != handle.pattern)) {
        return NULL;
    }

    return seq_project_get_track_const(project, handle.track);
}

static const seq_model_voice_t *_select_primary_voice(const seq_model_step_t *step) {
    if (step == NULL) {
        return NULL;
    }

    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = &step->voices[i];
        if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            return voice;
        }
    }

    return &step->voices[0];
}

bool seq_reader_get_step(seq_track_handle_t h, uint8_t step, seq_step_view_t *out) {
    if (out == NULL) {
        return false;
    }

    const seq_model_track_t *track = _resolve_legacy_track(h);
    if ((track == NULL) || (step >= SEQ_MODEL_STEPS_PER_TRACK)) {
        memset(out, 0, sizeof(*out));
        return false;
    }

    const seq_model_step_t *legacy_step = &track->steps[step];
    const seq_model_voice_t *voice = _select_primary_voice(legacy_step);

    memset(out, 0, sizeof(*out));

    if (voice != NULL) {
        out->note = voice->note;
        out->vel = voice->velocity;
        out->length = (uint16_t)voice->length;
        out->micro = voice->micro_offset;
    }

    if (legacy_step != NULL) {
        const bool has_voice = seq_model_step_has_playable_voice(legacy_step);
        const bool has_seq_plock = seq_model_step_has_seq_plock(legacy_step);
        const bool has_cart_plock = seq_model_step_has_cart_plock(legacy_step);
        const bool has_any_plock = has_seq_plock || has_cart_plock;
        const bool automation = seq_model_step_is_automation_only(legacy_step);

        uint8_t flags = 0U;
        if (has_voice) {
            flags |= SEQ_STEPF_HAS_VOICE;
        }
        if (has_any_plock) {
            flags |= SEQ_STEPF_HAS_ANY_PLOCK;
        }
        if (has_seq_plock) {
            flags |= SEQ_STEPF_HAS_SEQ_PLOCK;
        }
        if (has_cart_plock) {
            flags |= SEQ_STEPF_HAS_CART_PLOCK;
        }
        if (automation) {
            flags |= SEQ_STEPF_AUTOMATION_ONLY;
        }
        out->flags = flags;
    }

    return true;
}

bool seq_reader_get_step_voice(seq_track_handle_t h,
                               uint8_t step,
                               uint8_t voice_slot,
                               seq_step_voice_view_t *out) {
    if (out == NULL) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    const seq_model_track_t *track = _resolve_legacy_track(h);
    if ((track == NULL) ||
        (step >= SEQ_MODEL_STEPS_PER_TRACK) ||
        (voice_slot >= SEQ_MODEL_VOICES_PER_STEP)) {
        return false;
    }

    const seq_model_step_t *legacy_step = &track->steps[step];
    const seq_model_voice_t *voice = &legacy_step->voices[voice_slot];
    if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
        out->note = voice->note;
        out->vel = voice->velocity;
        out->length = voice->length;
        out->micro = voice->micro_offset;
        out->enabled = true;
    }

    return true;
}

bool seq_reader_count_step_voices(seq_track_handle_t h, uint8_t step, uint8_t *out_count) {
    if (out_count == NULL) {
        return false;
    }

    *out_count = 0U;

    const seq_model_track_t *track = _resolve_legacy_track(h);
    if ((track == NULL) || (step >= SEQ_MODEL_STEPS_PER_TRACK)) {
        return false;
    }

    const seq_model_step_t *legacy_step = &track->steps[step];
    uint8_t count = 0U;
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = &legacy_step->voices[i];
        if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            count++;
        }
    }

    *out_count = count;
    return true;
}

bool seq_reader_plock_iter_open(seq_track_handle_t h, uint8_t step, seq_plock_iter_t *it) {
    if (it == NULL) {
        return false;
    }

    const seq_model_step_t *step_model = seq_reader_peek_step(h, step);
    if (step_model == NULL) {
        it->_opaque = NULL;
        return false;
    }
    s_plock_iter_state.base = step_model->pl_ref.offset;
    s_plock_iter_state.count = step_model->pl_ref.count;
    if (s_plock_iter_state.count == 0U) {
        it->_opaque = NULL;
        return false;
    }
    s_plock_iter_state.i = 0U;
    it->_opaque = &s_plock_iter_state;
    return true;
}

bool seq_reader_plock_iter_next(seq_plock_iter_t *it, uint16_t *param_id, int32_t *value) {
    if ((it == NULL) || (it->_opaque == NULL)) {
        return false;
    }

    seq_reader_plock_iter_state_t *state = (seq_reader_plock_iter_state_t *)it->_opaque;
    if (state->i >= state->count) {
        return false;
    }

    const uint16_t absolute = (uint16_t)(state->base + state->i);
    const seq_plock_entry_t *entry = seq_plock_pool_get(absolute, 0U);
    state->i++;
    if (entry == NULL) {
        return false;
    }

    seq_reader_plock_item_t item;
    reader_pack_from_pool(entry, &item);

    if (param_id != NULL) {
        *param_id = _pool_encode_plock_id(item.param_id, item.flags);
    }
    if (value != NULL) {
        *value = _pool_decode_plock_value(item.value, item.flags);
    }
    return true;
}

int seq_reader_pl_open(seq_reader_pl_it_t *it, const seq_model_step_t *step) {
    if ((it == NULL) || (step == NULL)) {
        return 0;
    }

    it->offset = step->pl_ref.offset;
    it->count = step->pl_ref.count;
    it->index = 0U;
    return (it->count > 0U) ? 1 : 0;
}

int seq_reader_pl_next(seq_reader_pl_it_t *it, uint8_t *out_id, uint8_t *out_val, uint8_t *out_flags) {
    if ((it == NULL) || (it->index >= it->count)) {
        return 0;
    }

    const uint16_t absolute = (uint16_t)(it->offset + it->index);
    const seq_plock_entry_t *entry = seq_plock_pool_get(absolute, 0U);
    it->index++;
    if (entry == NULL) {
        return 0;
    }

    seq_reader_plock_item_t item;
    reader_pack_from_pool(entry, &item);

    if (out_id != NULL) {
        *out_id = item.param_id;
    }
    if (out_val != NULL) {
        *out_val = item.value;
    }
    if (out_flags != NULL) {
        *out_flags = item.flags;
    }
    return 1;
}

const seq_model_step_t *seq_reader_peek_step(seq_track_handle_t h, uint8_t step) {
    const seq_model_track_t *track = _resolve_legacy_track(h);
    if ((track == NULL) || (step >= SEQ_MODEL_STEPS_PER_TRACK)) {
        return NULL;
    }
    return &track->steps[step];
}

seq_track_handle_t seq_reader_get_active_track_handle(void) {
    seq_track_handle_t h = (seq_track_handle_t){0U, 0U, 0U};
    seq_cold_view_t project_view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    const seq_project_t *project = (const seq_project_t *)project_view._p;
    if ((project != NULL) && (project_view._bytes >= sizeof(*project))) {
        h.bank = seq_project_get_active_bank(project);
        h.pattern = seq_project_get_active_pattern_index(project);
        h.track = seq_project_get_active_track_index(project);
    }
    return h;
}
