#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apps/midi_probe.h"
#include "apps/runner_trace.h"
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

static void prepare_same_note_burst_pattern(void) {
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
    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        seq_model_step_t *s = &track->steps[step];
        s->voices[0].note = note;
        s->voices[0].length = 1U;
        s->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        s->voices[0].state = SEQ_MODEL_VOICE_ENABLED;

        switch (step & 0x03U) {
        case 0U:
            s->voices[0].length = 2U;
            s->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
            s->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
            break;
        case 1U:
            s->voices[0].length = 1U;
            s->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
            s->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
            break;
        case 2U:
            s->voices[0].length = 1U;
            s->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
            s->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
            break;
        default:
            s->voices[0].length = 1U;
            s->voices[0].velocity = 0U;
            s->voices[0].state = SEQ_MODEL_VOICE_DISABLED;
            break;
        }

        seq_model_step_recompute_flags(s);
    }

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
    runner_trace_reset();
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
    runner_trace_reset();
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

    size_t nominal_trace_count = runner_trace_count();
    assert(nominal_trace_count == 6U);
    const runner_trace_ev_t *ev0 = runner_trace_get(0U);
    const runner_trace_ev_t *ev1 = runner_trace_get(1U);
    const runner_trace_ev_t *ev2 = runner_trace_get(2U);
    const runner_trace_ev_t *ev3 = runner_trace_get(3U);
    const runner_trace_ev_t *ev4 = runner_trace_get(4U);
    const runner_trace_ev_t *ev5 = runner_trace_get(5U);
    assert(ev0 != NULL && ev0->type == 3U && ev0->step_abs == 0U);
    assert(ev1 != NULL && ev1->type == 1U && ev1->step_abs == 1U);
    assert(ev2 != NULL && ev2->type == 2U && ev2->step_abs == 1U);
    assert(ev3 != NULL && ev3->type == 3U && ev3->step_abs == 1U);
    assert(ev4 != NULL && ev4->type == 1U && ev4->step_abs == 2U);
    assert(ev5 != NULL && ev5->type == 2U && ev5->step_abs == 2U);

    midi_probe_reset();
    runner_trace_reset();
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
    assert(retrigger_events[1].ty == 2U); /* NOTE_OFF step 1 */
    assert(retrigger_events[2].ty == 1U); /* NOTE_ON step 1 */
    assert(retrigger_events[3].ty == 2U); /* NOTE_OFF step 2 */

    size_t retrigger_trace_count = runner_trace_count();
    assert(retrigger_trace_count == 5U);
    const runner_trace_ev_t *rt0 = runner_trace_get(0U);
    const runner_trace_ev_t *rt1 = runner_trace_get(1U);
    const runner_trace_ev_t *rt2 = runner_trace_get(2U);
    const runner_trace_ev_t *rt3 = runner_trace_get(3U);
    const runner_trace_ev_t *rt4 = runner_trace_get(4U);
    assert(rt0 != NULL && rt0->type == 3U && rt0->step_abs == 0U);
    assert(rt1 != NULL && rt1->type == 2U && rt1->step_abs == 1U);
    assert(rt2 != NULL && rt2->type == 3U && rt2->step_abs == 1U);
    assert(rt3 != NULL && rt3->type == 1U && rt3->step_abs == 2U);
    assert(rt4 != NULL && rt4->type == 2U && rt4->step_abs == 2U);

    for (unsigned i = 0U; i < retrigger_captured; ++i) {
        assert(retrigger_events[i].ch == 1U);
        assert(retrigger_events[i].note == 60U);
    }

    midi_probe_reset();
    runner_trace_reset();
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

    size_t edge_trace_count = runner_trace_count();
    assert(edge_trace_count == 6U);
    const runner_trace_ev_t *et0 = runner_trace_get(0U);
    const runner_trace_ev_t *et1 = runner_trace_get(1U);
    const runner_trace_ev_t *et2 = runner_trace_get(2U);
    const runner_trace_ev_t *et3 = runner_trace_get(3U);
    const runner_trace_ev_t *et4 = runner_trace_get(4U);
    const runner_trace_ev_t *et5 = runner_trace_get(5U);
    assert(et0 != NULL && et0->type == 3U && et0->step_abs == 0U);
    assert(et1 != NULL && et1->type == 1U && et1->step_abs == 1U);
    assert(et2 != NULL && et2->type == 2U && et2->step_abs == 1U);
    assert(et3 != NULL && et3->type == 4U && et3->step_abs == 1U);
    assert(et4 != NULL && et4->type == 1U && et4->step_abs == 2U);
    assert(et5 != NULL && et5->type == 2U && et5->step_abs == 2U);

    /* Burst test: mix explicit and implicit retriggers over 512 steps. */
    midi_probe_reset();
    runner_trace_reset();
    prepare_same_note_burst_pattern();
    seq_engine_runner_init();

    for (uint32_t t = 0U; t < 512U; ++t) {
        clock_step_info_t info = make_tick(t);
        seq_engine_runner_on_clock_step(&info);
    }

    unsigned burst_total = midi_probe_count();
    unsigned burst_silent = midi_probe_silent_ticks();
    unsigned burst_captured = 0U;
    const midi_probe_ev_t *burst_events = midi_probe_snapshot(&burst_captured);

    printf("runner_same_note_burst: events=%u silent_ticks=%u\n", burst_total, burst_silent);

    assert(burst_silent == 0U);
    unsigned burst_on = 0U;
    unsigned burst_off = 0U;
    for (unsigned i = 0U; i < burst_captured; ++i) {
        if (burst_events[i].ty == 1U) {
            ++burst_on;
        } else if (burst_events[i].ty == 2U) {
            ++burst_off;
        }
    }
    assert(burst_on == burst_off);

    size_t burst_trace = runner_trace_count();
    assert(burst_trace == 256U);
    bool seen_forced = false;
    bool seen_standard = false;
    for (size_t i = 0U; i < burst_trace; ++i) {
        const runner_trace_ev_t *ev = runner_trace_get(i);
        assert(ev != NULL);
        if (ev->type == 3U) {
            seen_standard = true;
        } else if (ev->type == 4U) {
            seen_forced = true;
        }
    }
    assert(seen_standard);
    assert(seen_forced);

    for (unsigned i = 0U; i < edge_captured; ++i) {
        assert(edge_events[i].ch == 1U);
        assert(edge_events[i].note == 60U);
    }

    return 0;
}
