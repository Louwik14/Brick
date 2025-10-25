#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch.h"
#include "apps/seq_led_bridge.h"
#include "apps/ui_keyboard_app.h"
#include "apps/seq_recorder.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_live_capture.h"
#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_runtime.h"
#include "core/clock_manager.h"
#include "midi/midi.h"
#include "ui/ui_led_backend.h"

/* ===== Stub runtime hooks ================================================= */
static systime_t g_stub_time;
static seq_led_runtime_t g_last_runtime;
static bool g_runtime_valid;
static uint16_t g_total_span;
static bool g_seq_running;
static uint32_t g_keyboard_note_on;
static uint32_t g_keyboard_note_off;
static uint32_t g_keyboard_all_notes_off;
static uint8_t g_keyboard_last_note_on;
static uint8_t g_keyboard_last_note_off;
static ui_led_mode_t g_keyboard_led_mode;
static bool g_keyboard_led_omni;

systime_t chVTGetSystemTimeX(void) {
    return g_stub_time;
}

systime_t chVTGetSystemTime(void) {
    return g_stub_time;
}

void chThdSleepMilliseconds(uint32_t ms) {
    g_stub_time += (systime_t)ms;
}

void chSysLock(void) {}
void chSysUnlock(void) {}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}

void ui_led_seq_update_from_app(const seq_led_runtime_t *rt) {
    if (rt != NULL) {
        g_last_runtime = *rt;
        g_runtime_valid = true;
    }
}

void ui_led_seq_set_total_span(uint16_t total_steps) {
    g_total_span = total_steps;
}

void ui_led_seq_set_running(bool running) {
    g_seq_running = running;
}

void ui_led_backend_set_mode(ui_led_mode_t mode) {
    g_keyboard_led_mode = mode;
}

void ui_led_backend_set_keyboard_omnichord(bool enabled) {
    g_keyboard_led_omni = enabled;
}

bool ui_mute_backend_is_muted(uint8_t track) {
    (void)track;
    return false;
}

void midi_note_on(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t vel) {
    (void)dest;
    (void)ch;
    (void)note;
    (void)vel;
}

void midi_note_off(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t vel) {
    (void)dest;
    (void)ch;
    (void)note;
    (void)vel;
}

void midi_cc(midi_dest_t dest, uint8_t ch, uint8_t cc, uint8_t val) {
    (void)dest;
    (void)ch;
    (void)cc;
    (void)val;
}

/* Not used by the bridge in these tests but required by the linker. */
void midi_all_notes_off(midi_dest_t dest, uint8_t ch) {
    (void)dest;
    (void)ch;
}

/* ===== Helpers ============================================================ */
static void reset_runtime(void) {
    g_stub_time = 100U;
    g_runtime_valid = false;
    g_total_span = 0U;
    g_seq_running = false;
    g_keyboard_note_on = 0U;
    g_keyboard_note_off = 0U;
    g_keyboard_all_notes_off = 0U;
    g_keyboard_last_note_on = 0U;
    g_keyboard_last_note_off = 0U;
    g_keyboard_led_mode = UI_LED_MODE_NONE;
    g_keyboard_led_omni = false;
    seq_runtime_init();
    seq_led_bridge_init();
    seq_project_t *project = seq_runtime_access_project_mut();
    if (project != NULL) {
        uint8_t active_bank = seq_project_get_active_bank(project);
        uint8_t active_pattern = seq_project_get_active_pattern_index(project);
        seq_led_bridge_set_active(active_bank, active_pattern);
    } else {
        seq_led_bridge_set_active(0U, 0U);
    }
    seq_led_bridge_bind_project(project);
    assert(g_runtime_valid);
}

static void set_stub_time(systime_t now) {
    g_stub_time = now;
}

static const seq_led_runtime_t *require_runtime(void) {
    assert(g_runtime_valid);
    return &g_last_runtime;
}

static void commit_hold_and_release(uint16_t held_mask, uint8_t step_index) {
    seq_led_bridge_plock_add(step_index);
    seq_led_bridge_begin_plock_preview(held_mask);
}

static void release_hold(uint16_t held_mask, uint8_t step_index) {
    (void)held_mask;
    seq_led_bridge_plock_remove(step_index);
    seq_led_bridge_end_plock_preview();
}

static void init_seq_recorder(void) {
    seq_model_track_t *track = seq_led_bridge_access_track();
    assert(track != NULL);
    seq_recorder_init(track);
    seq_recorder_set_recording(true);
}

/* ===== Tests ============================================================= */
static void test_seq_plock_commit_updates_step_flags(void) {
    reset_runtime();

    const uint16_t mask = 0x0001u;
    const uint8_t step_index = 0U;

    commit_hold_and_release(mask, step_index);
    seq_led_bridge_apply_plock_param(SEQ_HOLD_PARAM_V1_NOTE, 64, mask);
    release_hold(mask, step_index);

    const seq_model_track_t *track = seq_led_bridge_get_track();
    const seq_model_step_t *step = &track->steps[step_index];

    assert(seq_model_step_has_seq_plock(step));
    assert(seq_model_step_has_playable_voice(step));
    assert(!seq_model_step_is_automation_only(step));

    const seq_led_runtime_t *rt = require_runtime();
    assert(rt->steps[step_index].active);
    assert(!rt->steps[step_index].automation);
}

static void test_cart_plock_only_yields_automation_step(void) {
    reset_runtime();

    const uint16_t mask = 0x0001u;
    const uint8_t step_index = 0U;

    commit_hold_and_release(mask, step_index);
    seq_led_bridge_apply_cart_param(7U, 42, mask);
    release_hold(mask, step_index);

    const seq_model_track_t *track = seq_led_bridge_get_track();
    const seq_model_step_t *step = &track->steps[step_index];
    const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);

    assert(!seq_model_step_has_seq_plock(step));
    assert(seq_model_step_has_cart_plock(step));
    assert(seq_model_step_is_automation_only(step));
    assert(voice != NULL);
    assert(voice->velocity == 0U);

    const seq_led_runtime_t *rt = require_runtime();
    assert(!rt->steps[step_index].active);
    assert(rt->steps[step_index].automation);
}

static void test_seq_plock_keeps_velocity_and_length(void) {
    reset_runtime();

    const uint16_t mask = 0x0001u;
    const uint8_t step_index = 0U;

    commit_hold_and_release(mask, step_index);
    seq_led_bridge_apply_plock_param(SEQ_HOLD_PARAM_V1_VEL, 120, mask);
    seq_led_bridge_apply_plock_param(SEQ_HOLD_PARAM_V1_LEN, 12, mask);
    release_hold(mask, step_index);

    const seq_model_track_t *track = seq_led_bridge_get_track();
    const seq_model_step_t *step = &track->steps[step_index];
    const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);
    assert(voice != NULL);

    assert(seq_model_step_has_seq_plock(step));
    assert(seq_model_step_has_playable_voice(step));
    assert(!seq_model_step_is_automation_only(step));
    assert(voice->velocity == 120U);
    assert(voice->length == 12U);
}

static void test_seq_recorder_commits_length_and_led_state(void) {
    reset_runtime();
    init_seq_recorder();

    clock_step_info_t info = {
        .now = 0,
        .step_idx_abs = 0,
        .bpm = 120.0f,
        .tick_st = 100,
        .step_st = 600,
        .ext_clock = false,
    };

    seq_recorder_on_clock_step(&info);

    set_stub_time(50);
    seq_recorder_handle_note_on(60, 96);

    info.step_idx_abs = 1;
    info.now = 600;
    seq_recorder_on_clock_step(&info);

    set_stub_time(1250);
    seq_recorder_handle_note_off(60);

    const seq_model_track_t *track = seq_led_bridge_get_track();
    const seq_model_step_t *step = &track->steps[0];
    const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);
    assert(voice != NULL);
    assert(voice->velocity == 96U);
    assert(voice->length >= 2U);
    assert(seq_model_step_has_seq_plock(step));
    assert(seq_model_step_has_playable_voice(step));
    assert(!seq_model_step_is_automation_only(step));

    const seq_led_runtime_t *rt = require_runtime();
    assert(rt->steps[0].active);
    assert(!rt->steps[0].automation);
}

static void test_live_capture_records_length(void) {
    seq_model_track_t track;
    seq_model_track_init(&track);

    seq_live_capture_config_t cfg = { .track = &track };
    seq_live_capture_t capture;
    seq_live_capture_init(&capture, &cfg);
    seq_live_capture_set_recording(&capture, true);

    clock_step_info_t info = {
        .now = 0,
        .step_idx_abs = 0,
        .bpm = 120.0f,
        .tick_st = 100,
        .step_st = 600,
        .ext_clock = false
    };
    seq_live_capture_update_clock(&capture, &info);

    seq_live_capture_input_t on = {
        .type = SEQ_LIVE_CAPTURE_EVENT_NOTE_ON,
        .timestamp = 10,
        .note = 60,
        .velocity = 100,
        .voice_index = 0
    };
    seq_live_capture_plan_t plan;
    assert(seq_live_capture_plan_event(&capture, &on, &plan));
    assert(seq_live_capture_commit_plan(&capture, &plan));

    size_t recorded_step = plan.step_index;
    info.now = 600;
    info.step_idx_abs = 1;
    seq_live_capture_update_clock(&capture, &info);

    seq_live_capture_input_t off = {
        .type = SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF,
        .timestamp = 1210,
        .note = 60,
        .velocity = 0,
        .voice_index = 0
    };
    assert(seq_live_capture_plan_event(&capture, &off, &plan));
    assert(seq_live_capture_commit_plan(&capture, &plan));

    seq_model_step_t *step = &track.steps[recorded_step];
    const seq_model_voice_t *voice = seq_model_step_get_voice(step, 0U);
    assert(voice != NULL);
    assert(voice->length > 1U);

    bool has_length_plock = false;
    seq_reader_pl_it_t it;
    if (seq_reader_pl_open(&it, step) > 0) {
        uint8_t id = 0U;
        uint8_t raw_value = 0U;
        while (seq_reader_pl_next(&it, &id, &raw_value, NULL) != 0) {
            if (id == PL_INT_LEN_V0) {
                has_length_plock = true;
                assert(raw_value == voice->length);
                break;
            }
        }
    }
    assert(has_length_plock);
}

static void keyboard_sink_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch;
    (void)vel;
    g_keyboard_note_on++;
    g_keyboard_last_note_on = note;
}

static void keyboard_sink_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
    (void)ch;
    (void)vel;
    g_keyboard_note_off++;
    g_keyboard_last_note_off = note;
}

static void keyboard_sink_all_notes_off(uint8_t ch) {
    (void)ch;
    g_keyboard_all_notes_off++;
}

static void test_keyboard_note_off_does_not_emit_all_notes_off(void) {
    reset_runtime();

    ui_keyboard_note_sink_t sink = {
        .note_on = keyboard_sink_note_on,
        .note_off = keyboard_sink_note_off,
        .all_notes_off = keyboard_sink_all_notes_off,
        .midi_channel = 0U,
        .velocity = 100U,
    };

    ui_keyboard_app_init(&sink);
    assert(g_keyboard_led_mode == UI_LED_MODE_NONE);
    assert(!g_keyboard_led_omni);

    ui_keyboard_app_note_button(0U, true);
    ui_keyboard_app_note_button(0U, false);

    assert(g_keyboard_note_on == 1U);
    assert(g_keyboard_note_off == 1U);
    assert(g_keyboard_all_notes_off == 0U);
    assert(g_keyboard_last_note_on == g_keyboard_last_note_off);
}


int main(void) {
    test_seq_plock_commit_updates_step_flags();
    test_cart_plock_only_yields_automation_step();
    test_seq_plock_keeps_velocity_and_length();
    test_seq_recorder_commits_length_and_led_state();
    test_live_capture_records_length();
    test_keyboard_note_off_does_not_emit_all_notes_off();

    printf("seq_hold_runtime_tests: OK\n");
    return 0;
}
