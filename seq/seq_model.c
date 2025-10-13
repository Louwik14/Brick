/**
 * @file seq_model.c
 * @brief Implementation of the pure sequencer pattern model.
 * @ingroup seq_model
 */

#include "seq_model.h"

#include <string.h>

#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))

static void seq_model_bump_generation(seq_pattern_t *pattern) {
    if (!pattern) {
        return;
    }
    pattern->generation++;
    if (pattern->generation == 0) {
        pattern->generation = 1; /* avoid zero so snapshots can rely on it */
    }
}

static inline bool _voice_valid(uint8_t voice) {
    return voice < SEQ_MODEL_VOICE_COUNT;
}

static inline bool _step_valid(uint16_t step_idx) {
    return step_idx < SEQ_MODEL_STEP_COUNT;
}

static inline void seq_model_sync_params_from_base(seq_step_t *step) {
    if (!step) {
        return;
    }
    step->params[SEQ_PARAM_NOTE]         = step->note;
    step->params[SEQ_PARAM_VELOCITY]     = step->velocity;
    step->params[SEQ_PARAM_LENGTH]       = step->length;
    step->params[SEQ_PARAM_MICRO_TIMING] = step->micro_timing;
}

static inline int16_t clamp_param_value(seq_param_id_t param, int16_t value) {
    switch (param) {
        case SEQ_PARAM_NOTE:
            return (int16_t)CLAMP(value, 0, 127);
        case SEQ_PARAM_VELOCITY:
            return (int16_t)CLAMP(value, 0, 127);
        case SEQ_PARAM_LENGTH:
            return (int16_t)CLAMP(value, 1, SEQ_MODEL_STEP_COUNT);
        case SEQ_PARAM_MICRO_TIMING:
            return (int16_t)CLAMP(value, -SEQ_MODEL_MICRO_OFFSET_RANGE, SEQ_MODEL_MICRO_OFFSET_RANGE);
        default:
            return value;
    }
}

static inline int16_t base_param_value(const seq_step_t *step, seq_param_id_t param) {
    if (!step) {
        return 0;
    }
    switch (param) {
        case SEQ_PARAM_NOTE:
            return step->note;
        case SEQ_PARAM_VELOCITY:
            return step->velocity;
        case SEQ_PARAM_LENGTH:
            return step->length;
        case SEQ_PARAM_MICRO_TIMING:
            return step->micro_timing;
        default:
            return 0;
    }
}

void seq_model_init(seq_pattern_t *pattern) {
    if (!pattern) {
        return;
    }
    memset(pattern, 0, sizeof(*pattern));
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        pattern->voices[v].length = SEQ_MODEL_STEP_COUNT;
        for (uint16_t s = 0; s < SEQ_MODEL_STEP_COUNT; ++s) {
            seq_step_t *st = &pattern->voices[v].steps[s];
            st->note        = SEQ_MODEL_DEFAULT_NOTE; /* C4 */
            st->velocity    = SEQ_MODEL_DEFAULT_VELOCITY;
            st->length      = SEQ_MODEL_DEFAULT_LENGTH;
            st->micro_timing = SEQ_MODEL_DEFAULT_MICRO;
            st->active      = false;
            st->plock_mask  = 0;
            seq_model_sync_params_from_base(st);
        }
    }
    pattern->offsets.transpose   = 0;
    pattern->offsets.velocity    = 0;
    pattern->offsets.length      = 0;
    pattern->offsets.micro_timing= 0;
    pattern->generation          = 1;
}

void seq_model_clear(seq_pattern_t *pattern) {
    if (!pattern) {
        return;
    }
    seq_offsets_t offsets = pattern->offsets;
    seq_model_init(pattern);
    pattern->offsets = offsets;
    seq_model_bump_generation(pattern);
}

void seq_model_voice_set_length(seq_pattern_t *pattern, uint8_t voice, uint16_t length) {
    if (!pattern || !_voice_valid(voice)) {
        return;
    }
    if (length == 0 || length > SEQ_MODEL_STEP_COUNT) {
        length = SEQ_MODEL_STEP_COUNT;
    }
    pattern->voices[voice].length = length;
    seq_model_bump_generation(pattern);
}

uint16_t seq_model_voice_length(const seq_pattern_t *pattern, uint8_t voice) {
    if (!pattern || !_voice_valid(voice)) {
        return SEQ_MODEL_STEP_COUNT;
    }
    uint16_t len = pattern->voices[voice].length;
    return (len == 0) ? SEQ_MODEL_STEP_COUNT : len;
}

void seq_model_toggle_step(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    step->active = !step->active;
    if (!step->active) {
        /* Clear note-on data when toggled off */
        step->velocity = 0;
    } else if (step->velocity == 0) {
        step->velocity = 100;
    }
    seq_model_bump_generation(pattern);
}

void seq_model_set_step_active(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx, bool active) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    step->active = active;
    if (!active) {
        step->velocity = 0;
    } else if (step->velocity == 0) {
        step->velocity = 100;
    }
    seq_model_bump_generation(pattern);
}

bool seq_model_step_is_active(const seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return false;
    }
    const seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    return step->active;
}

void seq_model_set_step_param(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx,
                              seq_param_id_t param, int16_t value, bool enable_plock) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx) || param >= SEQ_PARAM_COUNT) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    int16_t clamped = clamp_param_value(param, value);

    if (enable_plock) {
        // FIX: enregistrer la valeur P-Lock réelle plutôt qu’un delta fantôme.
        step->params[param] = clamped;
        step->plock_mask |= (seq_plock_mask_t)(1u << param);
    } else {
        step->plock_mask &= (seq_plock_mask_t)~(1u << param);
        switch (param) {
            case SEQ_PARAM_NOTE:
                step->note = (uint8_t)clamped;
                break;
            case SEQ_PARAM_VELOCITY:
                step->velocity = (uint8_t)clamped;
                break;
            case SEQ_PARAM_LENGTH:
                step->length = (uint8_t)clamped;
                break;
            case SEQ_PARAM_MICRO_TIMING:
                step->micro_timing = (int8_t)clamped;
                break;
            default:
                break;
        }
        seq_model_sync_params_from_base(step);
    }
    seq_model_bump_generation(pattern);
}

int16_t seq_model_step_param(const seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx,
                             seq_param_id_t param, bool *is_plocked) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx) || param >= SEQ_PARAM_COUNT) {
        if (is_plocked) {
            *is_plocked = false;
        }
        return 0;
    }
    const seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    bool locked = (step->plock_mask & (seq_plock_mask_t)(1u << param)) != 0;
    if (is_plocked) {
        *is_plocked = locked;
    }
    return locked ? step->params[param] : base_param_value(step, param);
}

void seq_model_clear_step_params(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    step->plock_mask = 0;
    seq_model_sync_params_from_base(step);
    // FIX: retire les restes de P-Lock pour permettre le « quick clear ».
    seq_model_bump_generation(pattern);
}

void seq_model_step_clear_all(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    step->active       = false;
    step->note         = SEQ_MODEL_DEFAULT_NOTE;
    step->velocity     = SEQ_MODEL_DEFAULT_VELOCITY;
    step->length       = SEQ_MODEL_DEFAULT_LENGTH;
    step->micro_timing = SEQ_MODEL_DEFAULT_MICRO;
    step->plock_mask   = 0;
    seq_model_sync_params_from_base(step);
    seq_model_bump_generation(pattern);
}

void seq_model_set_offsets(seq_pattern_t *pattern, const seq_offsets_t *offsets) {
    if (!pattern || !offsets) {
        return;
    }
    pattern->offsets = *offsets;
    seq_model_bump_generation(pattern);
}

const seq_offsets_t *seq_model_get_offsets(const seq_pattern_t *pattern) {
    if (!pattern) {
        return NULL;
    }
    return &pattern->offsets;
}
