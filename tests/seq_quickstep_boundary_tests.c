#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apps/midi_probe.h"
#include "apps/quickstep_cache.h"
#include "apps/seq_engine_runner.h"
#include "cart/cart_bus.h"
#include "cart/cart_registry.h"
#include "core/clock_manager.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

static uint8_t g_active_bank = 0U;
static uint8_t g_active_pattern = 0U;

void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern) {
    g_active_bank = bank;
    g_active_pattern = pattern;
    quickstep_cache_set_active(bank, pattern);
    seq_project_t *project = seq_runtime_access_project_mut();
    if (project != NULL) {
        (void)seq_project_set_active_slot(project, bank, pattern);
    }
}

void seq_led_bridge_get_active(uint8_t *out_bank, uint8_t *out_pattern) {
    if (out_bank != NULL) {
        *out_bank = g_active_bank;
    }
    if (out_pattern != NULL) {
        *out_pattern = g_active_pattern;
    }
}

bool ui_mute_backend_is_muted(uint8_t track) {
    (void)track;
    return false;
}

void cart_link_param_changed(uint16_t param_id, uint8_t value, bool is_bitwise, uint8_t bit_mask) {
    (void)param_id;
    (void)value;
    (void)is_bitwise;
    (void)bit_mask;
}

uint8_t cart_link_shadow_get(cart_id_t cid, uint16_t param_id) {
    (void)cid;
    (void)param_id;
    return 0U;
}

void cart_link_shadow_set(cart_id_t cid, uint16_t param_id, uint8_t value) {
    (void)cid;
    (void)param_id;
    (void)value;
}

bool cart_set_param(cart_id_t id, uint16_t param, uint8_t value) {
    (void)id;
    (void)param;
    (void)value;
    return true;
}

cart_id_t cart_registry_get_active_id(void) {
    return CART1;
}

void cart_registry_init(void) {}

void cart_registry_register(cart_id_t id, const struct ui_cart_spec_t *ui_spec) {
    (void)id;
    (void)ui_spec;
}

const struct ui_cart_spec_t *cart_registry_get_ui_spec(cart_id_t id) {
    (void)id;
    return NULL;
}

const struct ui_cart_spec_t *cart_registry_switch(cart_id_t id) {
    (void)id;
    return NULL;
}

bool cart_registry_is_present(cart_id_t id) {
    (void)id;
    return false;
}

void cart_registry_set_uid(cart_id_t id, uint32_t uid) {
    (void)id;
    (void)uid;
}

uint32_t cart_registry_get_uid(cart_id_t id) {
    (void)id;
    return 0U;
}

bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out_id) {
    if (out_id != NULL) {
        *out_id = CART1;
    }
    (void)uid;
    return false;
}

static clock_step_info_t make_tick(uint32_t tick) {
    clock_step_info_t info = {
        .now = 0U,
        .step_idx_abs = tick,
        .bpm = 120.0f,
        .tick_st = 1U,
        .step_st = 6U,
        .ext_clock = false,
    };
    return info;
}

static void prepare_runtime(void) {
    seq_runtime_init();

    seq_project_t *project = seq_runtime_access_project_mut();
    assert(project != NULL);
    (void)seq_project_set_active_slot(project, 0U, 0U);
    (void)seq_project_set_active_track(project, 0U);

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);
    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        seq_model_step_init(&track->steps[step]);
    }
    seq_model_gen_reset(&track->generation);

    seq_led_bridge_set_active(0U, 0U);
}

static seq_model_track_t *active_track(void) {
    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);
    return track;
}

static void quickstep_arm_step(seq_model_track_t *track,
                               uint8_t step,
                               uint8_t note,
                               uint8_t velocity,
                               uint8_t length,
                               bool silent_view) {
    seq_model_step_t *dst = &track->steps[step];
    seq_model_step_init_default(dst, note);
    seq_model_voice_t *voice = &dst->voices[0];
    voice->note = note;
    voice->velocity = velocity;
    voice->length = (length == 0U) ? 1U : length;
    voice->state = (velocity > 0U) ? SEQ_MODEL_VOICE_ENABLED : SEQ_MODEL_VOICE_DISABLED;
    seq_model_step_recompute_flags(dst);

    quickstep_cache_mark(0U, 0U, 0U, step, 0U, note, velocity, voice->length);

    if (silent_view) {
        voice->velocity = 0U;
        voice->state = SEQ_MODEL_VOICE_DISABLED;
        seq_model_step_recompute_flags(dst);
    }

    seq_model_gen_bump(&track->generation);
}

static void run_ticks(uint32_t start_tick, uint32_t count) {
    for (uint32_t i = 0U; i < count; ++i) {
        const uint32_t tick = start_tick + i;
        clock_step_info_t info = make_tick(tick);
        seq_engine_runner_on_clock_step(&info);
    }
}

static void assert_sequence(uint8_t expected_types[], size_t expected_count, uint8_t expected_note) {
    unsigned event_count = 0U;
    const midi_probe_ev_t *events = midi_probe_snapshot(&event_count);
    assert(event_count == expected_count);
    for (size_t i = 0U; i < expected_count; ++i) {
        assert(events[i].ty == expected_types[i]);
        assert(events[i].note == expected_note);
        assert(events[i].ch == 1U);
    }
}

static void test_nominal_retrigger(void) {
    prepare_runtime();
    seq_engine_runner_init();
    seq_model_track_t *track = active_track();

    quickstep_arm_step(track, 0U, 64U, 100U, 1U, false);
    quickstep_arm_step(track, 1U, 64U, 100U, 1U, false);

    midi_probe_reset();

    run_ticks(0U, 3U);

    assert(midi_probe_silent_ticks() == 0U);
    uint8_t types[] = { 1U, 2U, 1U, 2U };
    assert_sequence(types, 4U, 64U);
}

static void test_length_two_then_one(void) {
    prepare_runtime();
    seq_engine_runner_init();
    seq_model_track_t *track = active_track();

    quickstep_arm_step(track, 0U, 64U, 100U, 2U, false);
    quickstep_arm_step(track, 1U, 64U, 100U, 1U, false);

    midi_probe_reset();

    run_ticks(0U, 3U);

    assert(midi_probe_silent_ticks() == 0U);
    uint8_t types[] = { 1U, 2U, 1U, 2U };
    assert_sequence(types, 4U, 64U);
}

static void test_rafale_cycles(void) {
    prepare_runtime();
    seq_engine_runner_init();
    seq_model_track_t *track = active_track();

    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        quickstep_arm_step(track, step, 65U, 100U, 1U, false);
    }

    midi_probe_reset();

    run_ticks(0U, 512U);

    assert(midi_probe_silent_ticks() == 0U);
    unsigned event_count = 0U;
    (void)midi_probe_snapshot(&event_count);
    assert(event_count > 0U);
}

static void test_quickstep_view_silent(void) {
    prepare_runtime();
    seq_engine_runner_init();
    seq_model_track_t *track = active_track();

    quickstep_arm_step(track, 0U, 67U, 96U, 1U, false);
    quickstep_arm_step(track, 1U, 67U, 96U, 1U, true);

    midi_probe_reset();

    run_ticks(0U, 3U);

    assert(midi_probe_silent_ticks() == 0U);
    uint8_t types[] = { 1U, 2U, 1U, 2U };
    assert_sequence(types, 4U, 67U);

    unsigned event_count = 0U;
    const midi_probe_ev_t *events = midi_probe_snapshot(&event_count);
    assert(event_count >= 3U);
    assert(events[2].vel == 96U);
}

int main(void) {
    test_nominal_retrigger();
    test_length_two_then_one();
    test_rafale_cycles();
    test_quickstep_view_silent();
    puts("OK");
    return 0;
}

