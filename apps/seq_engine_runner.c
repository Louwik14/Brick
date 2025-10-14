/**
 * @file seq_engine_runner.c
 * @brief Bridge between the sequencer engine and runtime I/O backends.
 */

#include "seq_engine_runner.h"

#include "cart_link.h"
#include "midi.h"
#include "seq_engine.h"
#include "ui_mute_backend.h"

#ifndef SEQ_ENGINE_RUNNER_MIDI_CHANNEL
#define SEQ_ENGINE_RUNNER_MIDI_CHANNEL 0U
#endif

static seq_engine_t s_engine;

static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time);
static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time);
static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time);
static bool _runner_track_muted(uint8_t track);
static uint8_t _runner_clamp_u8(int16_t value);

void seq_engine_runner_init(seq_model_pattern_t *pattern) {
    seq_engine_callbacks_t callbacks = {
        .note_on = _runner_note_on_cb,
        .note_off = _runner_note_off_cb,
        .plock = _runner_plock_cb
    };

    seq_engine_config_t config = {
        .pattern = pattern,
        .callbacks = callbacks,
        .is_track_muted = _runner_track_muted
    };

    seq_engine_init(&s_engine, &config);
}

void seq_engine_runner_attach_pattern(seq_model_pattern_t *pattern) {
    seq_engine_attach_pattern(&s_engine, pattern);
}

void seq_engine_runner_on_transport_play(void) {
    (void)seq_engine_start(&s_engine);
}

void seq_engine_runner_on_transport_stop(void) {
    seq_engine_stop(&s_engine);
}

void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
    seq_engine_process_step(&s_engine, info);
}

static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_on == NULL) {
        return MSG_OK;
    }
    midi_note_on(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_on->note, note_on->velocity);
    return MSG_OK;
}

static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_off == NULL) {
        return MSG_OK;
    }
    midi_note_off(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_off->note, 0U);
    return MSG_OK;
}

static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time) {
    (void)scheduled_time;
    if ((plock == NULL) || (plock->plock.domain != SEQ_MODEL_PLOCK_CART)) {
        return MSG_OK;
    }

    const seq_model_plock_t *payload = &plock->plock;
    const uint8_t value = _runner_clamp_u8(payload->value);
    cart_link_param_changed(payload->parameter_id, value, false, 0U);
    return MSG_OK;
}

static bool _runner_track_muted(uint8_t track) {
    return ui_mute_backend_is_muted(track);
}

static uint8_t _runner_clamp_u8(int16_t value) {
    if (value < 0) {
        return 0U;
    }
    if (value > 127) {
        return 127U;
    }
    return (uint8_t)value;
}
