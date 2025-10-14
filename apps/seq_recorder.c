/**
 * @file seq_recorder.c
 * @brief Live recording bridge connecting ui_keyboard_bridge to seq_live_capture.
 */

#include "seq_recorder.h"

#include "ch.h"

#include "core/seq/seq_live_capture.h"
#include "seq_led_bridge.h"
#include "core/seq/seq_model.h"

static seq_live_capture_t s_capture;
typedef struct {
    bool    active;
    uint8_t note;
} seq_recorder_active_voice_t;

static seq_recorder_active_voice_t s_active_voices[SEQ_MODEL_VOICES_PER_STEP];

static void _seq_recorder_reset_active_voices(void) {
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        s_active_voices[i].active = false;
        s_active_voices[i].note = 0U;
    }
}

static uint8_t _seq_recorder_reserve_slot(uint8_t note) {
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        if (s_active_voices[i].active && s_active_voices[i].note == note) {
            return i;
        }
    }
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        if (!s_active_voices[i].active) {
            return i;
        }
    }
    return 0U;
}

static int8_t _seq_recorder_lookup_slot(uint8_t note) {
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        if (s_active_voices[i].active && s_active_voices[i].note == note) {
            return (int8_t)i;
        }
    }
    return -1;
}

void seq_recorder_init(seq_model_pattern_t *pattern) {
    seq_live_capture_config_t cfg = {
        .pattern = pattern
    };
    seq_live_capture_init(&s_capture, &cfg);
    _seq_recorder_reset_active_voices();
}

void seq_recorder_attach_pattern(seq_model_pattern_t *pattern) {
    seq_live_capture_attach_pattern(&s_capture, pattern);
    _seq_recorder_reset_active_voices();
}

void seq_recorder_on_clock_step(const clock_step_info_t *info) {
    if (info == NULL) {
        return;
    }
    seq_live_capture_update_clock(&s_capture, info);
}

void seq_recorder_set_recording(bool enabled) {
    seq_live_capture_set_recording(&s_capture, enabled);
    if (!enabled) {
        _seq_recorder_reset_active_voices();
    }
}

void seq_recorder_handle_note_on(uint8_t note, uint8_t velocity) {
    seq_live_capture_input_t input;
    seq_live_capture_plan_t plan;

    input.type = SEQ_LIVE_CAPTURE_EVENT_NOTE_ON;
    input.timestamp = chVTGetSystemTimeX();
    input.note = note;
    input.velocity = velocity;
    input.voice_index = _seq_recorder_reserve_slot(note);

    if (!seq_live_capture_plan_event(&s_capture, &input, &plan)) {
        return;
    }

    if (plan.velocity == 0U) {
        return;
    }

    if (seq_live_capture_commit_plan(&s_capture, &plan)) {
        const uint8_t slot = input.voice_index % SEQ_MODEL_VOICES_PER_STEP;
        s_active_voices[slot].active = true;
        s_active_voices[slot].note = note;
        seq_led_bridge_publish();
    } else {
        const uint8_t slot = input.voice_index % SEQ_MODEL_VOICES_PER_STEP;
        if (slot < SEQ_MODEL_VOICES_PER_STEP) {
            s_active_voices[slot].active = false;
            s_active_voices[slot].note = 0U;
        }
    }
}

void seq_recorder_handle_note_off(uint8_t note) {
    seq_live_capture_input_t input;
    seq_live_capture_plan_t plan;

    input.type = SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF;
    input.timestamp = chVTGetSystemTimeX();
    input.note = note;
    input.velocity = 0U;

    int8_t slot = _seq_recorder_lookup_slot(note);
    input.voice_index = (slot >= 0) ? (uint8_t)slot : 0U;

    if (!seq_live_capture_plan_event(&s_capture, &input, &plan)) {
        return;
    }

    if (seq_live_capture_commit_plan(&s_capture, &plan)) {
        if (slot >= 0) {
            s_active_voices[(uint8_t)slot].active = false;
            s_active_voices[(uint8_t)slot].note = 0U;
        }
        seq_led_bridge_publish();
    }
}
