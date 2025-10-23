/**
 * @file seq_model.c
 * @brief Brick sequencer data model helpers.
 */

#include "seq_model.h"

#include <string.h>

static void seq_model_step_reset_offsets(seq_model_step_offsets_t *offsets);
static void seq_model_track_reset_config(seq_model_track_config_t *config);

void seq_model_gen_reset(seq_model_gen_t *gen) {
    if (gen == NULL) {
        return;
    }

    gen->value = 0U;
}

void seq_model_gen_bump(seq_model_gen_t *gen) {
    if (gen == NULL) {
        return;
    }

    ++gen->value;
}

bool seq_model_gen_has_changed(const seq_model_gen_t *lhs, const seq_model_gen_t *rhs) {
    if ((lhs == NULL) || (rhs == NULL)) {
        return false;
    }

    return lhs->value != rhs->value;
}

void seq_model_voice_init(seq_model_voice_t *voice, bool primary) {
    if (voice == NULL) {
        return;
    }

    voice->state = SEQ_MODEL_VOICE_DISABLED;
    voice->note = 60U;  /* C4 default. */
    voice->velocity = primary ? SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY : SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY;
    voice->length = 16U;
    voice->micro_offset = 0;
}

void seq_model_step_init(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    *step = k_seq_model_step_default;
#if SEQ_FEATURE_PLOCK_POOL
    step->pl_ref.offset = 0U;
    step->pl_ref.count = 0U;
#endif
    seq_model_step_recompute_flags(step);
}

void seq_model_step_init_default(seq_model_step_t *step, uint8_t note) {
    size_t i;

    if (step == NULL) {
        return;
    }

    seq_model_step_make_neutral(step);

    for (i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        seq_model_voice_t *voice = &step->voices[i];
        voice->note = note;
        if (i == 0U) {
            voice->state = SEQ_MODEL_VOICE_ENABLED;
        }
    }

#if SEQ_FEATURE_PLOCK_POOL
    step->pl_ref.offset = 0U;
    step->pl_ref.count = 0U;
#endif
    seq_model_step_recompute_flags(step);
}

void seq_model_track_init(seq_model_track_t *track) {
    size_t i;

    if (track == NULL) {
        return;
    }

    for (i = 0U; i < SEQ_MODEL_STEPS_PER_TRACK; ++i) {
        seq_model_step_init(&track->steps[i]);
    }

    seq_model_gen_reset(&track->generation);
    seq_model_track_reset_config(&track->config);
}

const seq_model_voice_t *seq_model_step_get_voice(const seq_model_step_t *step, size_t voice_index) {
    if ((step == NULL) || (voice_index >= SEQ_MODEL_VOICES_PER_STEP)) {
        return NULL;
    }

    return &step->voices[voice_index];
}

bool seq_model_step_set_voice(seq_model_step_t *step, size_t voice_index, const seq_model_voice_t *voice) {
    if ((step == NULL) || (voice == NULL) || (voice_index >= SEQ_MODEL_VOICES_PER_STEP)) {
        return false;
    }

    step->voices[voice_index] = *voice;
    seq_model_step_recompute_flags(step);
    return true;
}

bool seq_model_step_add_plock(seq_model_step_t *step, const seq_model_plock_t *plock) {
    if ((step == NULL) || (plock == NULL)) {
        return false;
    }

    if (step->plock_count >= SEQ_MODEL_MAX_PLOCKS_PER_STEP) {
        return false;
    }

    if (plock->voice_index >= SEQ_MODEL_VOICES_PER_STEP) {
        return false;
    }

    step->plocks[step->plock_count] = *plock;
    ++step->plock_count;
    seq_model_step_recompute_flags(step);
    return true;
}

void seq_model_step_clear_plocks(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    memset(step->plocks, 0, sizeof(step->plocks));
    step->plock_count = 0U;
#if SEQ_FEATURE_PLOCK_POOL
    step->pl_ref.offset = 0U;
    step->pl_ref.count = 0U;
#endif
    seq_model_step_recompute_flags(step);
}

bool seq_model_step_remove_plock(seq_model_step_t *step, size_t index) {
    if ((step == NULL) || (index >= step->plock_count)) {
        return false;
    }

    for (size_t i = index; i + 1U < step->plock_count; ++i) {
        step->plocks[i] = step->plocks[i + 1U];
    }
    memset(&step->plocks[step->plock_count - 1U], 0, sizeof(seq_model_plock_t));
    --step->plock_count;
    seq_model_step_recompute_flags(step);
    return true;
}

bool seq_model_step_get_plock(const seq_model_step_t *step, size_t index, seq_model_plock_t *out) {
    if ((step == NULL) || (index >= step->plock_count) || (out == NULL)) {
        return false;
    }

    *out = step->plocks[index];
    return true;
}

void seq_model_step_set_offsets(seq_model_step_t *step, const seq_model_step_offsets_t *offsets) {
    if ((step == NULL) || (offsets == NULL)) {
        return;
    }

    step->offsets = *offsets;
}

const seq_model_step_offsets_t *seq_model_step_get_offsets(const seq_model_step_t *step) {
    if (step == NULL) {
        return NULL;
    }

    return &step->offsets;
}

bool seq_model_step_has_playable_voice(const seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    return step->flags.active;
}

bool seq_model_step_is_automation_only(const seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    return step->flags.automation;
}

bool seq_model_step_has_any_plock(const seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    return step->plock_count > 0U;
}

bool seq_model_step_has_seq_plock(const seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plk = &step->plocks[i];
        if (plk->domain == SEQ_MODEL_PLOCK_INTERNAL) {
            return true;
        }
    }

    return false;
}

bool seq_model_step_has_cart_plock(const seq_model_step_t *step) {
    if (step == NULL) {
        return false;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plk = &step->plocks[i];
        if (plk->domain == SEQ_MODEL_PLOCK_CART) {
            return true;
        }
    }

    return false;
}

void seq_model_step_make_automation_only(seq_model_step_t *step) {
    size_t i;

    if (step == NULL) {
        return;
    }

    for (i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        seq_model_voice_t *voice = &step->voices[i];
        voice->state = SEQ_MODEL_VOICE_DISABLED;
        voice->velocity = 0U;
    }

    seq_model_step_recompute_flags(step);
}

void seq_model_step_make_neutral(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    seq_model_step_init(step);

    for (size_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        seq_model_voice_t *voice = &step->voices[i];
        voice->note = 60U;
        voice->length = 1U;
        voice->micro_offset = 0;
        if (i == 0U) {
            voice->velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
            voice->state = SEQ_MODEL_VOICE_ENABLED;
        } else {
            voice->velocity = SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY;
            voice->state = SEQ_MODEL_VOICE_DISABLED;
        }
    }

#if SEQ_FEATURE_PLOCK_POOL
    step->pl_ref.offset = 0U;
    step->pl_ref.count = 0U;
#endif
    seq_model_step_recompute_flags(step);
}

void seq_model_track_set_quantize(seq_model_track_t *track, const seq_model_quantize_config_t *config) {
    if ((track == NULL) || (config == NULL)) {
        return;
    }

    track->config.quantize = *config;
}

void seq_model_track_set_transpose(seq_model_track_t *track, const seq_model_transpose_config_t *config) {
    if ((track == NULL) || (config == NULL)) {
        return;
    }

    track->config.transpose = *config;
}

void seq_model_track_set_scale(seq_model_track_t *track, const seq_model_scale_config_t *config) {
    if ((track == NULL) || (config == NULL)) {
        return;
    }

    track->config.scale = *config;
}

static void seq_model_step_reset_offsets(seq_model_step_offsets_t *offsets) {
    if (offsets == NULL) {
        return;
    }

    offsets->transpose = 0;
    offsets->velocity = 0;
    offsets->length = 0;
    offsets->micro = 0;
}

static void seq_model_track_reset_config(seq_model_track_config_t *config) {
    if (config == NULL) {
        return;
    }

    *config = k_seq_model_track_config_default;
}

void seq_model_step_recompute_flags(seq_model_step_t *step) {
    if (step == NULL) {
        return;
    }

    bool has_voice = false;
    for (size_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = &step->voices[i];
        if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            has_voice = true;
            break;
        }
    }

    const bool has_seq_plock = seq_model_step_has_seq_plock(step);
    const bool has_cart_plock = seq_model_step_has_cart_plock(step);

    step->flags.active = has_voice;
    step->flags.automation = (!has_voice) && has_cart_plock && !has_seq_plock;
}
