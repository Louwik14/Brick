#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apps/midi_probe.h"
#include "apps/seq_engine_runner.h"
#include "cart/cart_bus.h"
#include "cart/cart_registry.h"
#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

/* -------------------------------------------------------------------------- */
/* Stubs (host)                                                               */
/* -------------------------------------------------------------------------- */

static uint8_t g_stub_active_bank = 0U;
static uint8_t g_stub_active_pattern = 0U;

void seq_led_bridge_set_active(uint8_t bank, uint8_t pattern) {
    g_stub_active_bank = bank;
    g_stub_active_pattern = pattern;
    seq_project_t *project = seq_runtime_access_project_mut();
    if (project != NULL) {
        (void)seq_project_set_active_slot(project, bank, pattern);
    }
}

void seq_led_bridge_get_active(uint8_t *out_bank, uint8_t *out_pattern) {
    if (out_bank != NULL) {
        *out_bank = g_stub_active_bank;
    }
    if (out_pattern != NULL) {
        *out_pattern = g_stub_active_pattern;
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

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static void prepare_pattern(void) {
    seq_runtime_init();

    seq_project_t *project = seq_runtime_access_project_mut();
    assert(project != NULL);
    (void)seq_project_set_active_slot(project, 0U, 0U);
    (void)seq_project_set_active_track(project, 0U);

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        seq_model_step_make_neutral(&track->steps[step]);
        track->steps[step].voices[0].note = (uint8_t)(60U + (step % 12U));
        track->steps[step].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        track->steps[step].voices[0].length = 1U;
        track->steps[step].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
        seq_model_step_recompute_flags(&track->steps[step]);
    }

    seq_model_gen_bump(&track->generation);

    seq_led_bridge_set_active(0U, 0U);
}

static void prepare_same_note_retrigger_pattern(void) {
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

    const uint8_t note = 60U;

    track->steps[0].voices[0].note = note;
    track->steps[0].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
    track->steps[0].voices[0].length = 2U;
    track->steps[0].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
    seq_model_step_recompute_flags(&track->steps[0]);

    track->steps[1].voices[0].note = note;
    track->steps[1].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
    track->steps[1].voices[0].length = 1U;
    track->steps[1].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
    seq_model_step_recompute_flags(&track->steps[1]);

    seq_model_gen_bump(&track->generation);

    seq_led_bridge_set_active(0U, 0U);
}

static void prepare_same_note_nominal_pattern(void) {
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

    const uint8_t note = 60U;

    track->steps[0].voices[0].note = note;
    track->steps[0].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
    track->steps[0].voices[0].length = 1U;
    track->steps[0].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
    seq_model_step_recompute_flags(&track->steps[0]);

    track->steps[1].voices[0].note = note;
    track->steps[1].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
    track->steps[1].voices[0].length = 1U;
    track->steps[1].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
    seq_model_step_recompute_flags(&track->steps[1]);

    seq_model_gen_bump(&track->generation);

    seq_led_bridge_set_active(0U, 0U);
}

static void prepare_same_note_retrigger_no_hit_pattern(void) {
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

    const uint8_t note = 60U;

    track->steps[0].voices[0].note = note;
    track->steps[0].voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
    track->steps[0].voices[0].length = 1U;
    track->steps[0].voices[0].state = SEQ_MODEL_VOICE_ENABLED;
    seq_model_step_recompute_flags(&track->steps[0]);

    track->steps[1].voices[0].note = note;
    track->steps[1].voices[0].velocity = 0U;
    track->steps[1].voices[0].length = 1U;
    track->steps[1].voices[0].state = SEQ_MODEL_VOICE_DISABLED;
    seq_model_step_recompute_flags(&track->steps[1]);

    seq_model_gen_bump(&track->generation);

    seq_led_bridge_set_active(0U, 0U);
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

/* -------------------------------------------------------------------------- */
/* Test                                                                       */
/* -------------------------------------------------------------------------- */

int main(void) {
    midi_probe_reset();
    prepare_pattern();
    seq_engine_runner_init();

    const uint32_t tick_count = 64U;
    for (uint32_t t = 0U; t < tick_count; ++t) {
        clock_step_info_t info = make_tick(t);
        seq_engine_runner_on_clock_step(&info);
    }

    unsigned total = midi_probe_count();
    unsigned silent = midi_probe_silent_ticks();
    unsigned captured = 0U;
    unsigned ons = 0U;
    unsigned offs = 0U;

    const midi_probe_ev_t *events = midi_probe_snapshot(&captured);
    for (unsigned i = 0U; i < captured; ++i) {
        if (events[i].ty == 1U) {
            ++ons;
        } else if (events[i].ty == 2U) {
            ++offs;
        }
    }

    printf("runner_smoke: events=%u silent_ticks=%u on=%u off=%u\n", total, silent, ons, offs);

    assert(total > 0U);
    assert(ons > 0U);
    assert(offs > 0U);
    assert(silent == 0U);

    midi_probe_reset();
    prepare_same_note_nominal_pattern();
    seq_engine_runner_init();

    for (uint32_t t = 0U; t < 3U; ++t) {
        clock_step_info_t info = make_tick(t);
        seq_engine_runner_on_clock_step(&info);
    }

    unsigned nominal_total = midi_probe_count();
    unsigned nominal_silent = midi_probe_silent_ticks();
    unsigned nominal_captured = 0U;
    const midi_probe_ev_t *nominal_events = midi_probe_snapshot(&nominal_captured);

    printf("runner_same_note_nominal: events=%u silent_ticks=%u\n", nominal_total, nominal_silent);

    assert(nominal_total == 4U);
    assert(nominal_captured == 4U);
    assert(nominal_silent == 0U);
    assert(nominal_events[0].ty == 1U);
    assert(nominal_events[1].ty == 2U);
    assert(nominal_events[2].ty == 1U);
    assert(nominal_events[3].ty == 2U);

    midi_probe_reset();
    prepare_same_note_retrigger_pattern();
    seq_engine_runner_init();

    for (uint32_t t = 0U; t < 3U; ++t) {
        clock_step_info_t info = make_tick(t);
        seq_engine_runner_on_clock_step(&info);
    }

    unsigned retrigger_total = midi_probe_count();
    unsigned retrigger_silent = midi_probe_silent_ticks();
    unsigned retrigger_captured = 0U;
    const midi_probe_ev_t *retrigger_events = midi_probe_snapshot(&retrigger_captured);

    printf("runner_same_note: events=%u silent_ticks=%u\n", retrigger_total, retrigger_silent);

    assert(retrigger_total == 4U);
    assert(retrigger_captured == 4U);
    assert(retrigger_silent == 0U);

    assert(retrigger_events[0].ty == 1U); /* NOTE_ON step 0 */
    assert(retrigger_events[1].ty == 2U); /* Forced NOTE_OFF at step 1 */
    assert(retrigger_events[2].ty == 1U); /* NOTE_ON step 1 */
    assert(retrigger_events[3].ty == 2U); /* NOTE_OFF step 2 */

    for (unsigned i = 0U; i < retrigger_captured; ++i) {
        assert(retrigger_events[i].ch == 1U);
        assert(retrigger_events[i].note == 60U);
    }

    midi_probe_reset();
    prepare_same_note_retrigger_no_hit_pattern();
    seq_engine_runner_init();

    for (uint32_t t = 0U; t < 3U; ++t) {
        clock_step_info_t info = make_tick(t);
        seq_engine_runner_on_clock_step(&info);
    }

    unsigned edge_total = midi_probe_count();
    unsigned edge_silent = midi_probe_silent_ticks();
    unsigned edge_captured = 0U;
    const midi_probe_ev_t *edge_events = midi_probe_snapshot(&edge_captured);

    printf("runner_same_note_edge_no_hit: events=%u silent_ticks=%u\n", edge_total, edge_silent);

    assert(edge_total == 4U);
    assert(edge_captured == 4U);
    assert(edge_silent == 0U);
    assert(edge_events[0].ty == 1U);
    assert(edge_events[1].ty == 2U);
    assert(edge_events[2].ty == 1U);
    assert(edge_events[3].ty == 2U);

    for (unsigned i = 0U; i < edge_captured; ++i) {
        assert(edge_events[i].ch == 1U);
        assert(edge_events[i].note == 60U);
    }

    return 0;
}
