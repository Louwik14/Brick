/**
 * @file seq_live_capture.c
 * @brief Live capture façade bridging UI inputs to the sequencer model.
 */

#include "seq_live_capture.h"

#include <string.h>

#include "ch.h"

#define MICRO_OFFSET_MIN   (-12)
#define MICRO_OFFSET_MAX   (12)

static void _seq_live_capture_reset_context(seq_live_capture_t *capture);
static void _seq_live_capture_bind_pattern(seq_live_capture_t *capture, seq_model_pattern_t *pattern);
static bool _seq_live_capture_compute_grid(const seq_live_capture_t *capture,
                                           seq_model_quantize_grid_t grid,
                                           systime_t *out_duration);
static void _seq_live_capture_divmod(int64_t value, int64_t divisor, int64_t *quotient, int64_t *remainder);
static int8_t _seq_live_capture_micro_from_delta(int64_t delta, int64_t step_duration);
static int8_t _seq_live_capture_micro_from_within(int64_t within_step, int64_t step_duration);
static size_t _seq_live_capture_wrap_step(int64_t base_step, int64_t delta);
static uint8_t _seq_live_capture_pick_voice_slot(const seq_model_step_t *step,
                                                 uint8_t requested,
                                                 uint8_t note);
static void _seq_live_capture_clear_voice_trackers(seq_live_capture_t *capture);
static bool _seq_live_capture_upsert_internal_plock(seq_model_step_t *step,
                                                    seq_model_plock_internal_param_t param,
                                                    uint8_t voice,
                                                    int32_t value);
static uint8_t _seq_live_capture_compute_length_steps(const seq_live_capture_t *capture,
                                                      systime_t start_time,
                                                      systime_t end_time,
                                                      systime_t step_duration_snapshot);

void seq_live_capture_init(seq_live_capture_t *capture, const seq_live_capture_config_t *config) {
    chDbgCheck(capture != NULL);

    _seq_live_capture_reset_context(capture);

    if ((config != NULL) && (config->pattern != NULL)) {
        _seq_live_capture_bind_pattern(capture, config->pattern);
    }
}

void seq_live_capture_attach_pattern(seq_live_capture_t *capture, seq_model_pattern_t *pattern) {
    chDbgCheck(capture != NULL);

    _seq_live_capture_bind_pattern(capture, pattern);
}

void seq_live_capture_override_quantize(seq_live_capture_t *capture, const seq_model_quantize_config_t *config) {
    chDbgCheck(capture != NULL);

    if (config != NULL) {
        capture->quantize = *config;
    }
}

void seq_live_capture_set_recording(seq_live_capture_t *capture, bool enabled) {
    chDbgCheck(capture != NULL);

    capture->recording = enabled;
}

bool seq_live_capture_is_recording(const seq_live_capture_t *capture) {
    if (capture == NULL) {
        return false;
    }

    return capture->recording;
}

void seq_live_capture_update_clock(seq_live_capture_t *capture, const clock_step_info_t *info) {
    chDbgCheck((capture != NULL) && (info != NULL));

    capture->clock_step_time = info->now;
    capture->clock_step_duration = info->step_st;
    capture->clock_tick_duration = info->tick_st;
    capture->clock_step_index = info->step_idx_abs;
    capture->clock_pattern_step = (size_t)(info->step_idx_abs % SEQ_MODEL_STEPS_PER_PATTERN);
    capture->clock_valid = true;
}

bool seq_live_capture_plan_event(seq_live_capture_t *capture,
                                 const seq_live_capture_input_t *input,
                                 seq_live_capture_plan_t *out_plan) {
    if ((capture == NULL) || (input == NULL) || (out_plan == NULL)) {
        return false;
    }

    memset(out_plan, 0, sizeof(*out_plan));

    if (!capture->recording || !capture->clock_valid) {
        return false;
    }

    if (capture->pattern == NULL) {
        return false;
    }

    if (capture->clock_step_duration == 0U) {
        return false;
    }

    seq_model_quantize_config_t active_quantize = capture->quantize;
    if (capture->pattern != NULL) {
        active_quantize = capture->pattern->config.quantize;
    }
    capture->quantize = active_quantize;

    int64_t base_time = (int64_t)capture->clock_step_time;
    int64_t step_duration = (int64_t)capture->clock_step_duration;
    int64_t delta_time = (int64_t)input->timestamp - base_time;
    int64_t base_step = (int64_t)capture->clock_pattern_step;

    if (delta_time < 0) {
        while (delta_time < 0) {
            delta_time += step_duration;
            base_time -= step_duration;
            base_step -= 1;
        }
    }

    int64_t applied_delta = delta_time;
    bool quantized = false;
    if (active_quantize.enabled && (active_quantize.strength > 0U)) {
        systime_t grid_duration = 0U;
        if (_seq_live_capture_compute_grid(capture, active_quantize.grid, &grid_duration) && (grid_duration > 0U)) {
            int64_t grid = (int64_t)grid_duration;
            int64_t rounded = ((delta_time + (grid / 2)) / grid) * grid;
            int64_t diff = rounded - delta_time;
            applied_delta = delta_time + (diff * (int64_t)active_quantize.strength) / 100;
            quantized = (diff != 0);
        }
    }

    int64_t quotient = 0;
    int64_t remainder = 0;
    _seq_live_capture_divmod(applied_delta, step_duration, &quotient, &remainder);

    size_t wrapped_step = _seq_live_capture_wrap_step(base_step, quotient);
    int64_t scheduled_time = base_time + applied_delta;

    int8_t micro_offset = _seq_live_capture_micro_from_within(remainder, step_duration);
    int8_t micro_adjust = _seq_live_capture_micro_from_delta(applied_delta - delta_time, step_duration);

    if (scheduled_time < 0) {
        scheduled_time = 0;
    }

    out_plan->type = input->type;
    out_plan->step_index = wrapped_step;
    out_plan->step_delta = (int32_t)quotient;
    out_plan->voice_index = input->voice_index;
    out_plan->note = input->note;
    out_plan->velocity = input->velocity;
    out_plan->micro_offset = micro_offset;
    out_plan->micro_adjust = micro_adjust;
    out_plan->quantized = quantized;
    out_plan->input_time = input->timestamp;
    out_plan->scheduled_time = (systime_t)scheduled_time;

    return true;
}

bool seq_live_capture_commit_plan(seq_live_capture_t *capture,
                                  const seq_live_capture_plan_t *plan) {
    if ((capture == NULL) || (plan == NULL) || (capture->pattern == NULL)) {
        return false;
    }

    if ((plan->type != SEQ_LIVE_CAPTURE_EVENT_NOTE_ON) &&
        (plan->type != SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF)) {
        return false;
    }

    if (plan->type == SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF) {
        if (plan->step_index >= SEQ_MODEL_STEPS_PER_PATTERN) {
            return false;
        }

        uint8_t slot = SEQ_MODEL_VOICES_PER_STEP;
        for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
            if (!capture->voices[i].active) {
                continue;
            }
            if ((capture->voices[i].note == plan->note) &&
                (capture->voices[i].voice_slot == plan->voice_index)) {
                slot = capture->voices[i].voice_slot;
                break;
            }
            if ((slot >= SEQ_MODEL_VOICES_PER_STEP) &&
                (capture->voices[i].note == plan->note)) {
                slot = capture->voices[i].voice_slot;
            }
        }
        if (slot >= SEQ_MODEL_VOICES_PER_STEP) {
            slot = (plan->voice_index < SEQ_MODEL_VOICES_PER_STEP) ? plan->voice_index : 0U;
        }

        size_t target_step = plan->step_index;
        if ((slot < SEQ_MODEL_VOICES_PER_STEP) && capture->voices[slot].active) {
            target_step = capture->voices[slot].step_index;
        }
        target_step %= SEQ_MODEL_STEPS_PER_PATTERN;

        seq_model_step_t *step = &capture->pattern->steps[target_step];
        const seq_model_voice_t *voice_src = seq_model_step_get_voice(step, slot);
        seq_model_voice_t voice;
        if (voice_src != NULL) {
            voice = *voice_src;
        } else {
            seq_model_voice_init(&voice, slot == 0U);
        }

        const systime_t start_time_raw = capture->voices[slot].active ?
                                          capture->voices[slot].start_time_raw : plan->input_time;
        const systime_t end_time_raw = plan->input_time;
        const systime_t start_step_duration = capture->voices[slot].active ?
                                              capture->voices[slot].step_duration : capture->clock_step_duration;
        const uint8_t length_steps = _seq_live_capture_compute_length_steps(capture,
                                                                            start_time_raw,
                                                                            end_time_raw,
                                                                            start_step_duration);

        if (voice.length != length_steps) {
            voice.length = length_steps;
        }
        if (voice.state != SEQ_MODEL_VOICE_ENABLED) {
            voice.state = (voice.velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
        }

        if (!seq_model_step_set_voice(step, slot, &voice)) {
            return false;
        }

        (void)_seq_live_capture_upsert_internal_plock(step,
                                                      SEQ_MODEL_PLOCK_PARAM_LENGTH,
                                                      slot,
                                                      length_steps);

        capture->voices[slot].active = false;
        capture->voices[slot].note = 0U;
        capture->voices[slot].start_time_raw = 0U;

        seq_model_gen_bump(&capture->pattern->generation);
        return true;
    }

    if (plan->step_index >= SEQ_MODEL_STEPS_PER_PATTERN) {
        return false;
    }

    seq_model_step_t *step = &capture->pattern->steps[plan->step_index];

    if (!seq_model_step_has_playable_voice(step) && !seq_model_step_has_any_plock(step)) {
        seq_model_step_make_automation_only(step); // --- FIX: éviter le C3 fantôme en gardant les voix désactivées ---
    }

    uint8_t slot = _seq_live_capture_pick_voice_slot(step, plan->voice_index, plan->note);
    const seq_model_voice_t *voice_src = seq_model_step_get_voice(step, slot);
    seq_model_voice_t voice;
    if (voice_src != NULL) {
        voice = *voice_src;
    } else {
        seq_model_voice_init(&voice, slot == 0U);
    }

    voice.note = plan->note;
    voice.velocity = plan->velocity;
    voice.state = (voice.velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
    if (voice.length == 0U) {
        voice.length = 1U;
    }
    voice.micro_offset = plan->micro_offset;

    if (!seq_model_step_set_voice(step, slot, &voice)) {
        return false;
    }

    (void)_seq_live_capture_upsert_internal_plock(step, SEQ_MODEL_PLOCK_PARAM_NOTE, slot, voice.note);
    (void)_seq_live_capture_upsert_internal_plock(step,
                                                  SEQ_MODEL_PLOCK_PARAM_VELOCITY,
                                                  slot,
                                                  voice.velocity);
    (void)_seq_live_capture_upsert_internal_plock(step,
                                                  SEQ_MODEL_PLOCK_PARAM_MICRO,
                                                  slot,
                                                  voice.micro_offset);

    capture->voices[slot].active = true;
    capture->voices[slot].step_index = plan->step_index;
    capture->voices[slot].start_time = plan->scheduled_time;
    capture->voices[slot].start_time_raw = plan->input_time;
    capture->voices[slot].step_duration = capture->clock_step_duration;
    capture->voices[slot].voice_slot = slot;
    capture->voices[slot].note = plan->note;

    seq_model_gen_bump(&capture->pattern->generation);
    return true;
}

static void _seq_live_capture_reset_context(seq_live_capture_t *capture) {
    memset(capture, 0, sizeof(*capture));
    capture->quantize.enabled = false;
    capture->quantize.grid = SEQ_MODEL_QUANTIZE_1_16;
    capture->quantize.strength = 100U;
    _seq_live_capture_clear_voice_trackers(capture);
}

static void _seq_live_capture_bind_pattern(seq_live_capture_t *capture, seq_model_pattern_t *pattern) {
    capture->pattern = pattern;
    if (pattern != NULL) {
        capture->quantize = pattern->config.quantize;
    }
    _seq_live_capture_clear_voice_trackers(capture);
}

static bool _seq_live_capture_compute_grid(const seq_live_capture_t *capture,
                                           seq_model_quantize_grid_t grid,
                                           systime_t *out_duration) {
    if ((capture == NULL) || (out_duration == NULL)) {
        return false;
    }

    uint32_t num = 0U;
    uint32_t den = 1U;

    switch (grid) {
        case SEQ_MODEL_QUANTIZE_1_4:
            num = 24U;
            break;
        case SEQ_MODEL_QUANTIZE_1_8:
            num = 12U;
            break;
        case SEQ_MODEL_QUANTIZE_1_16:
            num = 6U;
            break;
        case SEQ_MODEL_QUANTIZE_1_32:
            num = 3U;
            break;
        case SEQ_MODEL_QUANTIZE_1_64:
        default:
            num = 3U;
            den = 2U;
            break;
    }

    uint64_t tick = capture->clock_tick_duration;
    if (tick == 0U) {
        tick = capture->clock_step_duration / 6U;
    }

    if (tick == 0U) {
        return false;
    }

    uint64_t scaled = tick * num;
    if (den > 1U) {
        scaled = (scaled + (den / 2U)) / den;
    }

    if (scaled == 0U) {
        return false;
    }

    *out_duration = (systime_t)scaled;
    return true;
}

static void _seq_live_capture_divmod(int64_t value, int64_t divisor, int64_t *quotient, int64_t *remainder) {
    if (divisor == 0) {
        *quotient = 0;
        *remainder = 0;
        return;
    }

    int64_t q = value / divisor;
    int64_t r = value % divisor;

    if ((value < 0) && (r != 0)) {
        q -= 1;
        r += divisor;
    }

    *quotient = q;
    *remainder = r;
}

static int8_t _seq_live_capture_micro_from_delta(int64_t delta, int64_t step_duration) {
    if (step_duration == 0) {
        return 0;
    }

    int64_t scaled = (delta * MICRO_OFFSET_MAX) + ((delta >= 0) ? (step_duration / 2) : -(step_duration / 2));
    scaled /= step_duration;

    if (scaled > MICRO_OFFSET_MAX) {
        scaled = MICRO_OFFSET_MAX;
    } else if (scaled < MICRO_OFFSET_MIN) {
        scaled = MICRO_OFFSET_MIN;
    }

    return (int8_t)scaled;
}

static int8_t _seq_live_capture_micro_from_within(int64_t within_step, int64_t step_duration) {
    if (step_duration == 0) {
        return 0;
    }

    if (within_step < 0) {
        within_step = 0;
    }

    int64_t scaled = (within_step * MICRO_OFFSET_MAX) + (step_duration / 2);
    scaled /= step_duration;

    if (scaled > MICRO_OFFSET_MAX) {
        scaled = MICRO_OFFSET_MAX;
    } else if (scaled < MICRO_OFFSET_MIN) {
        scaled = MICRO_OFFSET_MIN;
    }

    return (int8_t)scaled;
}

static size_t _seq_live_capture_wrap_step(int64_t base_step, int64_t delta) {
    int64_t step = base_step + delta;
    while (step < 0) {
        step += SEQ_MODEL_STEPS_PER_PATTERN;
    }
    step %= SEQ_MODEL_STEPS_PER_PATTERN;
    return (size_t)step;
}

static uint8_t _seq_live_capture_pick_voice_slot(const seq_model_step_t *step,
                                                 uint8_t requested,
                                                 uint8_t note) {
    if (step == NULL) {
        return 0U;
    }

    if (requested < SEQ_MODEL_VOICES_PER_STEP) {
        const seq_model_voice_t *voice = seq_model_step_get_voice(step, requested);
        if ((voice == NULL) ||
            (voice->state != SEQ_MODEL_VOICE_ENABLED) ||
            (voice->velocity == 0U) ||
            (voice->note == note)) {
            return requested;
        }
    }

    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = seq_model_step_get_voice(step, i);
        if ((voice != NULL) && (voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->note == note)) {
            return i;
        }
    }

    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = seq_model_step_get_voice(step, i);
        if ((voice == NULL) || (voice->state != SEQ_MODEL_VOICE_ENABLED) || (voice->velocity == 0U)) {
            return i;
        }
    }

    return 0U;
}

static void _seq_live_capture_clear_voice_trackers(seq_live_capture_t *capture) {
    if (capture == NULL) {
        return;
    }

    for (size_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        capture->voices[i].active = false;
        capture->voices[i].step_index = 0U;
        capture->voices[i].start_time = 0U;
        capture->voices[i].start_time_raw = 0U;
        capture->voices[i].step_duration = 0U;
        capture->voices[i].voice_slot = (uint8_t)i;
        capture->voices[i].note = 0U;
    }
}

static bool _seq_live_capture_upsert_internal_plock(seq_model_step_t *step,
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
        .internal_param = param,
    };

    return seq_model_step_add_plock(step, &plock);
}

static uint8_t _seq_live_capture_compute_length_steps(const seq_live_capture_t *capture,
                                                      systime_t start_time,
                                                      systime_t end_time,
                                                      systime_t step_duration_snapshot) {
    (void)capture;
    if (step_duration_snapshot == 0U) {
        return 1U;
    }

    int64_t delta = (int64_t)end_time - (int64_t)start_time;
    if (delta <= 0) {
        return 1U;
    }

    int64_t step = (int64_t)step_duration_snapshot;
    int64_t length = (delta + (step / 2)) / step;
    if (length < 1) {
        length = 1;
    } else if (length > 64) {
        length = 64;
    }

    return (uint8_t)length;
}
