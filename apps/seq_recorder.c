/**
 * @file seq_recorder.c
 * @brief Live recording bridge connecting ui_keyboard_bridge to seq_live_capture.
 */

#include "seq_recorder.h"

#include "ch.h"

#include "core/seq/seq_live_capture.h"
#include "seq_led_bridge.h"

static seq_live_capture_t s_capture;

void seq_recorder_init(seq_model_pattern_t *pattern) {
    seq_live_capture_config_t cfg = {
        .pattern = pattern
    };
    seq_live_capture_init(&s_capture, &cfg);
}

void seq_recorder_attach_pattern(seq_model_pattern_t *pattern) {
    seq_live_capture_attach_pattern(&s_capture, pattern);
}

void seq_recorder_on_clock_step(const clock_step_info_t *info) {
    if (info == NULL) {
        return;
    }
    seq_live_capture_update_clock(&s_capture, info);
}

void seq_recorder_set_recording(bool enabled) {
    seq_live_capture_set_recording(&s_capture, enabled);
}

void seq_recorder_handle_note_on(uint8_t note, uint8_t velocity) {
    seq_live_capture_input_t input;
    seq_live_capture_plan_t plan;

    input.type = SEQ_LIVE_CAPTURE_EVENT_NOTE_ON;
    input.timestamp = chVTGetSystemTimeX();
    input.note = note;
    input.velocity = velocity;
    input.voice_index = 0U;

    if (!seq_live_capture_plan_event(&s_capture, &input, &plan)) {
        return;
    }

    if (plan.velocity == 0U) {
        return;
    }

    if (seq_live_capture_commit_plan(&s_capture, &plan)) {
        seq_led_bridge_publish();
    }
}

void seq_recorder_handle_note_off(uint8_t note) {
    (void)note;
    /* NOTE OFF capture deferred until player implementation. */
}
