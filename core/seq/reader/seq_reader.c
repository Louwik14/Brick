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
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_pool.h"
#endif

enum {
    k_seq_reader_plock_internal_flag = 0x8000U,
    k_seq_reader_plock_internal_voice_shift = 8U,
    k_seq_reader_pl_flag_domain_cart = 0x01U,
    k_seq_reader_pl_flag_signed = 0x02U,
    k_seq_reader_pl_flag_voice_shift = 2U,
};

typedef struct {
    const seq_model_plock_t *plocks;
    uint8_t count;
    uint8_t index;
} seq_reader_plock_iter_state_t;

static seq_reader_plock_iter_state_t s_plock_iter_state;

static int16_t _clamp_i16(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint8_t _legacy_encode_internal_id(const seq_model_plock_t *plock) {
    if (plock == NULL) {
        return 0U;
    }

    switch (plock->internal_param) {
        case SEQ_MODEL_PLOCK_PARAM_NOTE:
            return (uint8_t)(PL_INT_NOTE_V0 + (plock->voice_index & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_VELOCITY:
            return (uint8_t)(PL_INT_VEL_V0 + (plock->voice_index & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_LENGTH:
            return (uint8_t)(PL_INT_LEN_V0 + (plock->voice_index & 0x03U));
        case SEQ_MODEL_PLOCK_PARAM_MICRO:
            return (uint8_t)(PL_INT_MIC_V0 + (plock->voice_index & 0x03U));
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
        *flags = (uint8_t)(*flags | k_seq_reader_pl_flag_signed);
    }
    int16_t clamped = _clamp_i16(value, -128, 127);
    return pl_u8_from_s8((int8_t)clamped);
}

static uint8_t _encode_unsigned_value(int16_t value, int16_t min_value, int16_t max_value) {
    int16_t clamped = _clamp_i16(value, min_value, max_value);
    if (clamped < 0) {
        clamped = 0;
    }
    return (uint8_t)(clamped & 0x00FF);
}

static void _legacy_extract_plock_payload(const seq_model_plock_t *plock,
                                          uint8_t *out_id,
                                          uint8_t *out_val,
                                          uint8_t *out_flags) {
    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flags = 0U;

    if (plock == NULL) {
        if (out_id != NULL) {
            *out_id = 0U;
        }
        if (out_val != NULL) {
            *out_val = 0U;
        }
        if (out_flags != NULL) {
            *out_flags = 0U;
        }
        return;
    }

    if (plock->domain == SEQ_MODEL_PLOCK_CART) {
        id = (uint8_t)(plock->parameter_id & 0x00FFU);
        value = _encode_unsigned_value(plock->value, 0, 255);
        flags |= k_seq_reader_pl_flag_domain_cart;
    } else {
        id = _legacy_encode_internal_id(plock);
        switch (plock->internal_param) {
            case SEQ_MODEL_PLOCK_PARAM_NOTE:
                value = _encode_unsigned_value(plock->value, 0, 127);
                flags = (uint8_t)(flags | ((plock->voice_index & 0x03U) << k_seq_reader_pl_flag_voice_shift));
                break;
            case SEQ_MODEL_PLOCK_PARAM_VELOCITY:
                value = _encode_unsigned_value(plock->value, 0, 127);
                flags = (uint8_t)(flags | ((plock->voice_index & 0x03U) << k_seq_reader_pl_flag_voice_shift));
                break;
            case SEQ_MODEL_PLOCK_PARAM_LENGTH:
                value = _encode_unsigned_value(plock->value, 0, 255);
                flags = (uint8_t)(flags | ((plock->voice_index & 0x03U) << k_seq_reader_pl_flag_voice_shift));
                break;
            case SEQ_MODEL_PLOCK_PARAM_MICRO:
                value = _encode_signed_value(plock->value, &flags);
                flags = (uint8_t)(flags | ((plock->voice_index & 0x03U) << k_seq_reader_pl_flag_voice_shift));
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

    if (out_id != NULL) {
        *out_id = id;
    }
    if (out_val != NULL) {
        *out_val = value;
    }
    if (out_flags != NULL) {
        *out_flags = flags;
    }
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

static uint16_t _encode_plock_id(const seq_model_plock_t *plock) {
    if (plock == NULL) {
        return 0U;
    }

    if (plock->domain == SEQ_MODEL_PLOCK_CART) {
        return plock->parameter_id;
    }

    const uint16_t voice = ((uint16_t)(plock->voice_index & 0x03U)) << k_seq_reader_plock_internal_voice_shift;
    const uint16_t param = (uint16_t)plock->internal_param & 0x00FFU;
    return (uint16_t)(k_seq_reader_plock_internal_flag | voice | param);
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

    const seq_model_track_t *track = _resolve_legacy_track(h);
    if ((track == NULL) || (step >= SEQ_MODEL_STEPS_PER_TRACK)) {
        it->_opaque = NULL;
        return false;
    }

    const seq_model_step_t *legacy_step = &track->steps[step];
    s_plock_iter_state.plocks = legacy_step->plocks;
    s_plock_iter_state.count = legacy_step->plock_count;
    s_plock_iter_state.index = 0U;
    it->_opaque = &s_plock_iter_state;
    return true;
}

bool seq_reader_plock_iter_next(seq_plock_iter_t *it, uint16_t *param_id, int32_t *value) {
    if ((it == NULL) || (it->_opaque == NULL)) {
        return false;
    }

    seq_reader_plock_iter_state_t *state = (seq_reader_plock_iter_state_t *)it->_opaque;
    if ((state->plocks == NULL) || (state->index >= state->count)) {
        return false;
    }

    const seq_model_plock_t *plock = &state->plocks[state->index];
    state->index++;

    if (param_id != NULL) {
        *param_id = _encode_plock_id(plock);
    }
    if (value != NULL) {
        *value = (int32_t)plock->value;
    }

    return true;
}

int seq_reader_pl_open(seq_reader_pl_it_t *it, const seq_model_step_t *step) {
    if ((it == NULL) || (step == NULL)) {
        return 0;
    }

    it->step = step;
    it->i = 0U;
    it->off = 0U;
    it->n = 0U;
    it->use_pool = 0U;

#if SEQ_FEATURE_PLOCK_POOL
    if (step->pl_ref.count > 0U) {
        it->use_pool = 1U;
        it->off = step->pl_ref.offset;
        it->n = step->pl_ref.count;
        return (it->n > 0U) ? 1 : 0;
    }
#endif

    it->n = step->plock_count;
    return (it->n > 0U) ? 1 : 0;
}

int seq_reader_pl_next(seq_reader_pl_it_t *it, uint8_t *out_id, uint8_t *out_val, uint8_t *out_flags) {
    if ((it == NULL) || (it->step == NULL) || (it->i >= it->n)) {
        return 0;
    }

#if SEQ_FEATURE_PLOCK_POOL
    if (it->use_pool) {
        const seq_plock_entry_t *entry = seq_plock_pool_get(it->off, it->i);
        if (entry == NULL) {
            return 0;
        }
        it->i++;
        if (out_id != NULL) {
            *out_id = entry->param_id;
        }
        if (out_val != NULL) {
            *out_val = entry->value;
        }
        if (out_flags != NULL) {
            *out_flags = entry->flags;
        }
        return 1;
    }
#endif

    const seq_model_plock_t *plock = &it->step->plocks[it->i];
    it->i++;
    _legacy_extract_plock_payload(plock, out_id, out_val, out_flags);
    return 1;
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
