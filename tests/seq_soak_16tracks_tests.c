#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ch.h"
#include "core/clock_manager.h"
#include "core/seq/seq_engine.h"
#include "core/seq/seq_model.h"
#include "tests/support/rt_blackbox.h"

#define SOAK_TRACK_COUNT 16U
#define SOAK_STEP_DURATION 24U
#ifndef SOAK_TICKS
#define SOAK_TICKS 10000U
#endif

static unsigned g_total_events = 0U;
static uint32_t g_current_tick = 0U;
static uint8_t g_dispatch_track = 0U;
static uint8_t g_dispatch_step = 0U;
static uint8_t g_step_index_per_track[SOAK_TRACK_COUNT];

static msg_t host_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_on != NULL) {
        bb_pair_on(g_dispatch_track, note_on->note, g_current_tick);
    }
    bb_track_on(g_dispatch_track);
    bb_log(g_current_tick, g_dispatch_track, g_dispatch_step, 1U);
    ++g_total_events;
    return MSG_OK;
}

static msg_t host_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_off != NULL) {
        bb_pair_off(g_dispatch_track, note_off->note, g_current_tick);
    }
    bb_track_off(g_dispatch_track);
    bb_log(g_current_tick, g_dispatch_track, g_dispatch_step, 2U);
    ++g_total_events;
    return MSG_OK;
}

static msg_t host_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time) {
    (void)plock;
    (void)scheduled_time;
    return MSG_OK;
}

typedef struct {
    seq_model_track_t track;
    seq_engine_t engine;
} track_ctx_t;

static void host_dispatch_event(uint8_t track_index, seq_engine_t *engine, const seq_engine_event_t *event) {
    if ((engine == NULL) || (event == NULL)) {
        return;
    }

    g_dispatch_track = track_index;
    g_dispatch_step = g_step_index_per_track[track_index];

    switch (event->type) {
    case SEQ_ENGINE_EVENT_NOTE_ON: {
        const seq_engine_note_on_t *note_on = &event->data.note_on;
        if (note_on->voice < SEQ_MODEL_VOICES_PER_STEP) {
            engine->voice_active[note_on->voice] = true;
            engine->voice_note[note_on->voice] = note_on->note;
        }
        if (engine->config.callbacks.note_on != NULL) {
            engine->config.callbacks.note_on(note_on, event->scheduled_time);
        }
        break;
    }
    case SEQ_ENGINE_EVENT_NOTE_OFF: {
        const seq_engine_note_off_t *note_off = &event->data.note_off;
        if (note_off->voice < SEQ_MODEL_VOICES_PER_STEP) {
            engine->voice_active[note_off->voice] = false;
            engine->voice_note[note_off->voice] = note_off->note;
        }
        if (engine->config.callbacks.note_off != NULL) {
            engine->config.callbacks.note_off(note_off, event->scheduled_time);
        }
        break;
    }
    case SEQ_ENGINE_EVENT_PLOCK: {
        if (engine->config.callbacks.plock != NULL) {
            engine->config.callbacks.plock(&event->data.plock, event->scheduled_time);
        }
        break;
    }
    default:
        break;
    }
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
}

int main(void) {
    track_ctx_t ctx[SOAK_TRACK_COUNT];
    memset(ctx, 0, sizeof(ctx));

    bb_reset();
    bb_pair_reset();
    bb_track_counters_reset();

    seq_engine_callbacks_t callbacks = {
        .note_on = host_note_on_cb,
        .note_off = host_note_off_cb,
        .plock = host_plock_cb,
    };

    for (uint8_t i = 0U; i < SOAK_TRACK_COUNT; ++i) {
        init_track_pattern(&ctx[i], i);

        seq_engine_config_t config = {
            .track = &ctx[i].track,
            .callbacks = callbacks,
            .is_track_muted = NULL,
        };

        seq_engine_init(&ctx[i].engine, &config);
        seq_engine_set_callbacks(&ctx[i].engine, &callbacks);
        seq_engine_attach_track(&ctx[i].engine, &ctx[i].track);
        ctx[i].engine.clock_attached = true;
        ctx[i].engine.player.running = true;
    }

    systime_t current_time = 0U;
    ch_stub_set_time(current_time);

    for (uint32_t tick = 0U; tick < SOAK_TICKS; ++tick) {
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
            g_step_index_per_track[t] = (uint8_t)(tick % SEQ_MODEL_STEPS_PER_TRACK);
            seq_engine_process_step(&ctx[t].engine, &info);
        }

        current_time += SOAK_STEP_DURATION;
        ch_stub_set_time(current_time);

        for (uint8_t t = 0U; t < SOAK_TRACK_COUNT; ++t) {
            seq_engine_event_t event;
            while (seq_engine_scheduler_peek(&ctx[t].engine.scheduler, &event) &&
                   (event.scheduled_time <= current_time)) {
                (void)seq_engine_scheduler_pop(&ctx[t].engine.scheduler, &event);
                host_dispatch_event(t, &ctx[t].engine, &event);
            }
        }

        bb_tick_end();
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
