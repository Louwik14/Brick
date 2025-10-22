#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "apps/midi_probe.h"
#include "apps/seq_engine_runner.h"
#include "apps/seq_led_bridge.h"
#include "cart/cart_registry.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* -------------------------------------------------------------------------- */
/* Stubs                                                                       */
/* -------------------------------------------------------------------------- */

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
/* Helpers                                                                     */
/* -------------------------------------------------------------------------- */

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

static void setup_environment(void) {
    midi_probe_reset();
    seq_runtime_init();
    seq_led_bridge_init();
    seq_engine_runner_init();

    seq_project_t *project = seq_runtime_access_project_mut();
    assert(project != NULL);
    (void)seq_project_set_active_slot(project, 0U, 0U);
    (void)seq_project_set_active_track(project, 0U);

    seq_led_bridge_bind_project(project);
    seq_led_bridge_set_active(0U, 0U);
    assert(seq_led_bridge_select_track(0U));

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);
    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        seq_model_step_init(&track->steps[step]);
    }
    seq_model_gen_bump(&track->generation);
}

static void arm_quickstep_pair(void) {
    seq_led_bridge_quick_toggle_step(0);
    seq_led_bridge_quick_toggle_step(1);
}

static void assert_boundary_retrigger(uint32_t total_ticks, unsigned expected_cycles) {
    unsigned captured = 0U;
    const midi_probe_ev_t *events = midi_probe_snapshot(&captured);

    const unsigned expected_events = expected_cycles * 4U;
    assert(captured == expected_events);

    for (unsigned cycle = 0U; cycle < expected_cycles; ++cycle) {
        const midi_probe_ev_t *group = &events[cycle * 4U];
        assert(group[0].ty == 1U);
        assert(group[1].ty == 2U);
        assert(group[2].ty == 1U);
        assert(group[3].ty == 2U);
        assert(group[0].note == group[2].note);
        assert(group[0].vel > 0U);
        assert(group[2].vel > 0U);
    }

    const unsigned silent = midi_probe_silent_ticks();
    const unsigned expected_silent = total_ticks - (expected_cycles * 3U);
    assert(silent == expected_silent);
}

static void run_ticks(uint32_t total_ticks) {
    for (uint32_t tick = 0U; tick < total_ticks; ++tick) {
        clock_step_info_t info = make_tick(tick);
        seq_engine_runner_on_clock_step(&info);
    }
}

static void scenario_quickstep_boundary(void) {
    setup_environment();

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    arm_quickstep_pair();

    const uint32_t total_ticks = 512U;
    const unsigned cycles = total_ticks / SEQ_MODEL_STEPS_PER_TRACK;
    run_ticks(total_ticks);

    assert_boundary_retrigger(total_ticks, cycles);

    /* ensure we played two note-ons per cycle */
    unsigned captured = 0U;
    const midi_probe_ev_t *events = midi_probe_snapshot(&captured);
    unsigned on_events = 0U;
    for (unsigned i = 0U; i < captured; ++i) {
        if (events[i].ty == 1U) {
            ++on_events;
        }
    }
    assert(on_events == (2U * cycles));

    (void)track;
}

static void scenario_quickstep_view_silent(void) {
    setup_environment();

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    arm_quickstep_pair();

    seq_model_step_t *step1 = &track->steps[1];
    step1->voices[0].velocity = 0U;
    step1->voices[0].state = SEQ_MODEL_VOICE_DISABLED;
    seq_model_step_recompute_flags(step1);

    const uint32_t total_ticks = SEQ_MODEL_STEPS_PER_TRACK;
    const unsigned cycles = total_ticks / SEQ_MODEL_STEPS_PER_TRACK;
    run_ticks(total_ticks);

    unsigned captured = 0U;
    const midi_probe_ev_t *events = midi_probe_snapshot(&captured);

    unsigned on_count = 0U;
    for (unsigned i = 0U; i < captured; ++i) {
        if (events[i].ty == 1U) {
            ++on_count;
        }
    }

    assert(on_count >= 2U);
    assert(captured >= 3U);
    assert(events[2].ty == 1U);
    assert(events[2].vel > 0U);

    (void)cycles;
}

int main(void) {
    scenario_quickstep_boundary();
    scenario_quickstep_view_silent();

    printf("quickstep_boundary: ok\n");
    return 0;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

