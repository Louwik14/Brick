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
    out_plan->scheduled_time = (systime_t)scheduled_time;

    return true;
}

static void _seq_live_capture_reset_context(seq_live_capture_t *capture) {
    memset(capture, 0, sizeof(*capture));
    capture->quantize.enabled = false;
    capture->quantize.grid = SEQ_MODEL_QUANTIZE_1_16;
    capture->quantize.strength = 100U;
}

static void _seq_live_capture_bind_pattern(seq_live_capture_t *capture, seq_model_pattern_t *pattern) {
    capture->pattern = pattern;
    if (pattern != NULL) {
        capture->quantize = pattern->config.quantize;
    }
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
