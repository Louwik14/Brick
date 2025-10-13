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

void seq_model_init(seq_pattern_t *pattern) {
    if (!pattern) {
        return;
    }
    memset(pattern, 0, sizeof(*pattern));
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        pattern->voices[v].length = SEQ_MODEL_STEP_COUNT;
        for (uint16_t s = 0; s < SEQ_MODEL_STEP_COUNT; ++s) {
            seq_step_t *st = &pattern->voices[v].steps[s];
            st->note        = 60; /* C4 default */
            st->velocity    = 100;
            st->length      = 1;
            st->micro_timing = 0;
            st->active      = false;
            st->plock_mask  = 0;
            memset(st->params, 0, sizeof(st->params));
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

static inline void _write_param(seq_step_t *step, seq_param_id_t param, int16_t value) {
    switch (param) {
        case SEQ_PARAM_NOTE:
            step->params[param] = CLAMP(value, 0, 127);
            break;
        case SEQ_PARAM_VELOCITY:
            step->params[param] = CLAMP(value, 0, 127);
            break;
        case SEQ_PARAM_LENGTH:
            step->params[param] = CLAMP(value, 1, SEQ_MODEL_STEP_COUNT);
            break;
        case SEQ_PARAM_MICRO_TIMING:
            step->params[param] = CLAMP(value, -SEQ_MODEL_MICRO_OFFSET_RANGE, SEQ_MODEL_MICRO_OFFSET_RANGE);
            break;
        default:
            break;
    }
}

void seq_model_set_step_param(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx,
                              seq_param_id_t param, int16_t value, bool enable_plock) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx) || param >= SEQ_PARAM_COUNT) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    _write_param(step, param, value);
    if (enable_plock) {
        step->plock_mask |= (seq_plock_mask_t)(1u << param);
    } else {
        step->plock_mask &= (seq_plock_mask_t)~(1u << param);
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
    if (is_plocked) {
        *is_plocked = ((step->plock_mask & (seq_plock_mask_t)(1u << param)) != 0);
    }
    return step->params[param];
}

void seq_model_clear_step_params(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx) {
    if (!pattern || !_voice_valid(voice) || !_step_valid(step_idx)) {
        return;
    }
    seq_step_t *step = &pattern->voices[voice].steps[step_idx];
    step->plock_mask = 0;
    memset(step->params, 0, sizeof(step->params));
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
