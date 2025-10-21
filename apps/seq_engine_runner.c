/**
 * @file seq_engine_runner.c
 * @brief Reader-only runner translating sequencer steps into MIDI events.
 */

#include "seq_engine_runner.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "core/seq/seq_access.h"
#include "midi_helpers.h"
#include "ui_mute_backend.h"

#ifndef SEQ_MAX_ACTIVE_TRACKS
#define SEQ_MAX_ACTIVE_TRACKS 16U
#endif

typedef struct {
    uint8_t bank;
    uint8_t pattern;
} runner_active_t;

static volatile runner_active_t s_act = {0U, 0U};

typedef struct {
    bool active;
    uint8_t note;
    uint8_t remaining_steps;
} runner_pending_note_t;

static runner_pending_note_t s_pending[SEQ_MAX_ACTIVE_TRACKS];
static bool s_running = false;

static void runner_flush_pending(void);
static void runner_reset_pending(void);
static void runner_abort_track(uint8_t track);
static void runner_tick_pending(void);
static uint8_t runner_clamp_length(uint16_t length);
static void runner_schedule_note_off(uint8_t track, uint8_t note, uint8_t length_steps);

void seq_runner_set_active_pattern(uint8_t bank, uint8_t pattern) {
    s_act.bank = bank;
    s_act.pattern = pattern;
}

void seq_engine_runner_init(void) {
    s_running = false;
    runner_reset_pending();
}

void seq_engine_runner_on_transport_play(void) {
    runner_flush_pending();
    runner_reset_pending();
    s_running = true;
}

void seq_engine_runner_on_transport_stop(void) {
    runner_flush_pending();
    runner_reset_pending();
    s_running = false;
    for (uint8_t ch = 1U; ch <= SEQ_MAX_ACTIVE_TRACKS; ++ch) {
        midi_send_all_notes_off(ch);
    }
}

void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
    if ((info == NULL) || !s_running) {
        return;
    }

    runner_tick_pending();

    const uint8_t bank = s_act.bank;
    const uint8_t pattern = s_act.pattern;
    const uint8_t step = (uint8_t)(info->step_idx_abs % SEQ_MODEL_STEPS_PER_TRACK);

    for (uint8_t track = 0U; track < SEQ_MAX_ACTIVE_TRACKS; ++track) {
        if (ui_mute_backend_is_muted(track)) {
            runner_abort_track(track);
            continue;
        }

        seq_track_handle_t handle = seq_reader_make_handle(bank, pattern, track);
        seq_step_view_t view;
        if (!seq_reader_get_step(handle, step, &view)) {
            runner_abort_track(track);
            continue;
        }

        if ((view.flags & SEQ_STEPF_HAS_VOICE) == 0U) {
            continue;
        }

        if (view.vel == 0U) {
            runner_abort_track(track);
            continue;
        }

        const uint8_t channel = (uint8_t)(track + 1U);
        midi_send_note_on(channel, view.note, view.vel);

        const uint8_t length_steps = runner_clamp_length(view.length);
        runner_schedule_note_off(track, view.note, length_steps);
    }
}

static void runner_flush_pending(void) {
    for (uint8_t track = 0U; track < SEQ_MAX_ACTIVE_TRACKS; ++track) {
        runner_pending_note_t *slot = &s_pending[track];
        if (slot->active) {
            midi_send_note_off((uint8_t)(track + 1U), slot->note);
            slot->active = false;
            slot->note = 0U;
            slot->remaining_steps = 0U;
        }
    }
}

static void runner_reset_pending(void) {
    memset(s_pending, 0, sizeof(s_pending));
}

static void runner_abort_track(uint8_t track) {
    if (track >= SEQ_MAX_ACTIVE_TRACKS) {
        return;
    }
    runner_pending_note_t *slot = &s_pending[track];
    if (!slot->active) {
        return;
    }
    midi_send_note_off((uint8_t)(track + 1U), slot->note);
    slot->active = false;
    slot->note = 0U;
    slot->remaining_steps = 0U;
}

static void runner_tick_pending(void) {
    for (uint8_t track = 0U; track < SEQ_MAX_ACTIVE_TRACKS; ++track) {
        runner_pending_note_t *slot = &s_pending[track];
        if (!slot->active) {
            continue;
        }
        if (slot->remaining_steps > 0U) {
            slot->remaining_steps--;
        }
        if (slot->remaining_steps == 0U) {
            midi_send_note_off((uint8_t)(track + 1U), slot->note);
            slot->active = false;
            slot->note = 0U;
        }
    }
}

static uint8_t runner_clamp_length(uint16_t length) {
    if (length == 0U) {
        return 1U;
    }
    if (length > SEQ_MODEL_STEPS_PER_TRACK) {
        return SEQ_MODEL_STEPS_PER_TRACK;
    }
    return (uint8_t)length;
}

static void runner_schedule_note_off(uint8_t track, uint8_t note, uint8_t length_steps) {
    if (track >= SEQ_MAX_ACTIVE_TRACKS) {
        return;
    }
    if (length_steps == 0U) {
        length_steps = 1U;
    }

    runner_pending_note_t *slot = &s_pending[track];
    if (slot->active) {
        midi_send_note_off((uint8_t)(track + 1U), slot->note);
    }

    slot->active = true;
    slot->note = note;
    slot->remaining_steps = length_steps;
}
