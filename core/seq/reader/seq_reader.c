#include "core/seq/reader/seq_reader.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/seq_runtime.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_model.h"

enum {
    k_seq_step_view_flag_active = 1U << 0,
    k_seq_step_view_flag_automation = 1U << 1,
    k_seq_step_view_flag_has_plock = 1U << 2,
    k_seq_reader_plock_internal_flag = 0x8000U,
    k_seq_reader_plock_internal_voice_shift = 8U,
};

typedef struct {
    const seq_model_plock_t *plocks;
    uint8_t count;
    uint8_t index;
} seq_reader_plock_iter_state_t;

static seq_reader_plock_iter_state_t s_plock_iter_state;

static const seq_model_track_t *_resolve_legacy_track(seq_track_handle_t handle) {
    if ((handle.bank >= SEQ_PROJECT_BANK_COUNT) ||
        (handle.pattern >= SEQ_PROJECT_PATTERNS_PER_BANK) ||
        (handle.track >= SEQ_PROJECT_MAX_TRACKS)) {
        return NULL;
    }

    const seq_project_t *project = seq_runtime_get_project();
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
        if (legacy_step->flags.active != 0U) {
            out->flags |= k_seq_step_view_flag_active;
        }
        if (legacy_step->flags.automation != 0U) {
            out->flags |= k_seq_step_view_flag_automation;
        }
        if (legacy_step->plock_count > 0U) {
            out->flags |= k_seq_step_view_flag_has_plock;
        }
    }

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
