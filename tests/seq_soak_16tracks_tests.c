#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "core/clock_manager.h"
#include "core/seq/seq_model.h"
#include "tests/support/rt_blackbox.h"
#include "tests/support/rt_queues.h"
#include "tests/support/rt_timing.h"
#include "tests/support/seq_rt_runs.h"

#define SOAK_TRACK_COUNT 16U
#define SOAK_STEP_DURATION 24U
#ifndef SOAK_TICKS
#define SOAK_TICKS 10000U
#endif

typedef struct {
    bool active;
    uint8_t note;
    uint32_t off_step;
} track_note_state_t;

typedef struct {
    seq_model_track_t track;
    track_note_state_t note_state;
} track_ctx_t;

static unsigned g_total_events = 0U;
static uint32_t g_current_tick = 0U;

static void emit_note_on(uint8_t track_index, uint8_t step_index, uint8_t note) {
    rq_player_enq();
    bb_pair_on(track_index, note, g_current_tick);
    bb_track_on(track_index);
    bb_log(g_current_tick, track_index, step_index, 1U);
    ++g_total_events;
    rq_player_deq();
}

static void emit_note_off(uint8_t track_index, uint8_t step_index, uint8_t note) {
    rq_player_enq();
    bb_pair_off(track_index, note, g_current_tick);
    bb_track_off(track_index);
    bb_log(g_current_tick, track_index, step_index, 2U);
    ++g_total_events;
    rq_player_deq();
}

static const seq_model_voice_t *select_primary_voice(const seq_model_step_t *step) {
    if (step == NULL) {
        return NULL;
    }
    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        const seq_model_voice_t *voice = &step->voices[v];
        if ((voice->state == SEQ_MODEL_VOICE_ENABLED) && (voice->velocity > 0U)) {
            return voice;
        }
    }
    return NULL;
}

static void init_track_pattern(track_ctx_t *ctx, uint8_t track_index) {
    seq_model_track_init(&ctx->track);

    for (uint8_t step = 0U; step < SEQ_MODEL_STEPS_PER_TRACK; ++step) {
        if ((step % 4U) != (track_index % 4U)) {
            continue;
        }

        seq_model_step_t *slot = &ctx->track.steps[step];
        seq_model_step_make_neutral(slot);
        slot->voices[0].note = (uint8_t)(60U + track_index);
        slot->voices[0].velocity = SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY;
        slot->voices[0].length = 1U;
        slot->voices[0].state = SEQ_MODEL_VOICE_ENABLED;
        seq_model_step_recompute_flags(slot);
    }

    seq_model_gen_bump(&ctx->track.generation);
    ctx->note_state = (track_note_state_t){0};
}

static void process_track_step(track_ctx_t *ctx, uint8_t track_index, const clock_step_info_t *info) {
    if ((ctx == NULL) || (info == NULL)) {
        return;
    }

    track_note_state_t *state = &ctx->note_state;
    const uint32_t step_abs = info->step_idx_abs;
    const uint8_t step_idx = (uint8_t)(step_abs % SEQ_MODEL_STEPS_PER_TRACK);

    if (state->active && (step_abs >= state->off_step)) {
        emit_note_off(track_index, step_idx, state->note);
        state->active = false;
    }

    seq_model_step_t *step = &ctx->track.steps[step_idx];
    if (!seq_model_step_has_playable_voice(step)) {
        return;
    }

    const seq_model_voice_t *voice = select_primary_voice(step);
    if (voice == NULL) {
        return;
    }

    if (state->active) {
        emit_note_off(track_index, step_idx, state->note);
        state->active = false;
    }

    emit_note_on(track_index, step_idx, voice->note);

    state->active = true;
    state->note = voice->note;
    uint32_t length = voice->length;
    if (length == 0U) {
        length = 1U;
    }
    state->off_step = step_abs + length;
}

int seq_rt_run_16tracks_soak(void) {
    track_ctx_t ctx[SOAK_TRACK_COUNT];
    memset(ctx, 0, sizeof(ctx));

    bb_reset();
    bb_pair_reset();
    bb_track_counters_reset();
    rq_reset();
    rt_tim_reset();

    for (uint8_t i = 0U; i < SOAK_TRACK_COUNT; ++i) {
        init_track_pattern(&ctx[i], i);
    }

    systime_t current_time = 0U;
    ch_stub_set_time(current_time);

    for (uint32_t tick = 0U; tick < SOAK_TICKS; ++tick) {
        rt_tim_tick_begin();
        g_current_tick = tick;
        bb_tick_begin(g_current_tick);

        clock_step_info_t info = {
            .now = current_time,
            .step_idx_abs = tick,
            .bpm = 120.0f,
            .tick_st = 1U,
            .step_st = SOAK_STEP_DURATION,
            .ext_clock = false,
        };

        for (uint8_t t = 0U; t < SOAK_TRACK_COUNT; ++t) {
            process_track_step(&ctx[t], t, &info);
        }

        bb_tick_end();
        rt_tim_tick_end();

        current_time += SOAK_STEP_DURATION;
        ch_stub_set_time(current_time);
    }

    g_current_tick = SOAK_TICKS;
    for (uint8_t t = 0U; t < SOAK_TRACK_COUNT; ++t) {
        track_note_state_t *state = &ctx[t].note_state;
        if (state->active) {
            uint8_t step_idx = (uint8_t)(state->off_step % SEQ_MODEL_STEPS_PER_TRACK);
            emit_note_off(t, step_idx, state->note);
            state->active = false;
        }
    }

    double avg = (SOAK_TICKS > 0U) ? ((double)g_total_events / (double)SOAK_TICKS) : 0.0;
    const unsigned silent = bb_silent_ticks();
    unsigned u_on = bb_unmatched_on();
    unsigned u_off = bb_unmatched_off();
    uint32_t maxlen = bb_max_note_len_ticks();

    printf("16-track soak: ticks=%u total_events=%u silent_ticks=%u unmatched_on=%u unmatched_off=%u max_len_ticks=%lu events_per_tick=%.2f\n",
           (unsigned)SOAK_TICKS,
           g_total_events,
           silent,
           u_on,
           u_off,
           (unsigned long)maxlen,
           avg);

    rt_tim_report();
    rq_report();

    const double P99_BUDGET_NS = 2000000.0;
    if (rt_tim_p99_ns() > P99_BUDGET_NS) {
        fprintf(stderr,
                "Regression: p99 tick time %.0f ns > %.0f ns\n",
                rt_tim_p99_ns(),
                P99_BUDGET_NS);
        bb_dump();
        return EXIT_FAILURE;
    }

    const unsigned MIN_TRACKS_ACTIVE = 16U;
    const unsigned MAX_SILENT_TICKS = 0U;
    const uint32_t MAX_REASONABLE_LEN = 64U;

    unsigned total_on = 0U;
    unsigned total_off = 0U;
    unsigned tracks_active = 0U;
    for (int tr = 0; tr < (int)SOAK_TRACK_COUNT; ++tr) {
        unsigned on = bb_track_on_count((uint8_t)tr);
        unsigned off = bb_track_off_count((uint8_t)tr);
        if ((on != 0U) || (off != 0U)) {
            ++tracks_active;
        }
        total_on += on;
        total_off += off;
    }

    printf("tracks_active=%u total_on=%u total_off=%u\n", tracks_active, total_on, total_off);

    if (silent > MAX_SILENT_TICKS) {
        fprintf(stderr, "Regression: silent ticks detected (%u > %u)\n", silent, MAX_SILENT_TICKS);
        bb_dump();
        return EXIT_FAILURE;
    }

    if (rq_any_underflow_or_overflow() != 0) {
        fprintf(stderr, "Regression: RT queue underflow/overflow detected\n");
        bb_dump();
        return EXIT_FAILURE;
    }

    if ((u_on != 0U) || (u_off != 0U)) {
        fprintf(stderr,
                "Regression: MIDI pairing invariant violated (unmatched_on=%u unmatched_off=%u)\n",
                u_on,
                u_off);
        bb_dump();
        return EXIT_FAILURE;
    }

    if (maxlen > MAX_REASONABLE_LEN) {
        fprintf(stderr,
                "Regression: note length too large (%lu > %lu ticks)\n",
                (unsigned long)maxlen,
                (unsigned long)MAX_REASONABLE_LEN);
        bb_dump();
        return EXIT_FAILURE;
    }

    if (tracks_active < MIN_TRACKS_ACTIVE) {
        fprintf(stderr,
                "Regression: only %u tracks active (< %u)\n",
                tracks_active,
                MIN_TRACKS_ACTIVE);
        bb_dump();
        return EXIT_FAILURE;
    }

    if (g_total_events == 0U) {
        fprintf(stderr, "Regression: no events captured during soak test\n");
        bb_dump();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#if !defined(SEQ_RT_TEST_LIBRARY)
int main(void) {
    return seq_rt_run_16tracks_soak();
}
#endif
