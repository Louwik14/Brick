#include <assert.h>
#include <stdint.h>

#include "apps/seq_engine_runner.h"
#include "core/seq/seq_access.h"
#include "midi.h"
#include "tests/support/rt_blackbox.h"
#include "ui/ui_mute_backend.h"

static uint32_t g_stub_tick = 0U;
static uint8_t g_stub_step = 0U;

void midi_note_on(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t velocity) {
    (void)dest;
    bb_track_on(ch);
    bb_log(g_stub_tick, ch, g_stub_step, 1U);
    bb_pair_on(ch, note, g_stub_tick);
    (void)velocity;
}

void midi_note_off(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t velocity) {
    (void)dest;
    (void)velocity;
    bb_track_off(ch);
    bb_log(g_stub_tick, ch, g_stub_step, 2U);
    bb_pair_off(ch, note, g_stub_tick);
}

void midi_all_notes_off(midi_dest_t dest, uint8_t ch) {
    (void)dest;
    (void)ch;
}

static void populate_track(seq_model_track_t *track) {
    if (track == NULL) {
        return;
    }
    seq_model_track_init(track);
    for (uint8_t step = 0U; step < 8U; ++step) {
        seq_model_step_t *slot = &track->steps[step];
        seq_model_step_make_neutral(slot);
        slot->voices[0].note = (uint8_t)(60U + step);
        slot->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        slot->voices[0].length = 1U;
        slot->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
        seq_model_step_recompute_flags(slot);
    }
    seq_model_gen_bump(&track->generation);
}

static void run_ticks(uint32_t tick_count) {
    bb_reset();
    for (uint32_t tick = 0U; tick < tick_count; ++tick) {
        g_stub_tick = tick;
        g_stub_step = (uint8_t)(tick % SEQ_MODEL_STEPS_PER_TRACK);
        bb_tick_begin(tick);
        clock_step_info_t info = {
            .now = 0U,
            .step_idx_abs = tick,
            .bpm = 120.0f,
            .tick_st = 1U,
            .step_st = 6U,
            .ext_clock = false,
        };
        seq_engine_runner_on_clock_step(&info);
        bb_tick_end();
    }
}

static void assert_no_silent_ticks(void) {
    assert(bb_silent_ticks() == 0U);
    assert(bb_unmatched_on() == 0U);
    assert(bb_unmatched_off() == 0U);
}

int main(void) {
    seq_runtime_init();
    ui_mute_backend_init();

    seq_model_track_t *track0 = seq_runtime_access_track_mut(0U);
    populate_track(track0);

    seq_runner_set_active_pattern(0U, 0U);
    seq_engine_runner_init();

    seq_engine_runner_on_transport_play();
    run_ticks(8U);
    seq_engine_runner_on_transport_stop();
    assert_no_silent_ticks();

    seq_engine_runner_on_transport_play();
    run_ticks(8U);
    seq_engine_runner_on_transport_stop();
    assert_no_silent_ticks();

    return 0;
}
