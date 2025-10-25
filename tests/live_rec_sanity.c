#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/seq/seq_live_capture.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_pool.h"
#include "core/seq/seq_plock_ids.h"

static clock_step_info_t make_clock(uint32_t step_idx, systime_t now) {
    clock_step_info_t info;
    memset(&info, 0, sizeof(info));
    info.step_idx_abs = step_idx;
    info.now = now;
    info.tick_st = 1U;
    info.step_st = 6U;
    info.bpm = 120.0f;
    info.ext_clock = false;
    return info;
}

static bool step_has_velocity(const seq_model_step_t *step, uint8_t expected) {
    if (step == NULL) {
        return false;
    }
    const uint8_t count = seq_model_step_plock_count(step);
    for (uint8_t i = 0U; i < count; ++i) {
        const plk2_t *entry = seq_model_step_get_plock(step, i);
        if ((entry != NULL) && (entry->param_id == PL_INT_VEL_V0)) {
            return entry->value == expected;
        }
    }
    return false;
}

static bool capture_has_active_voice(const seq_live_capture_t *capture) {
    if (capture == NULL) {
        return false;
    }
    for (uint8_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        if (capture->voices[i].active) {
            return true;
        }
    }
    return false;
}

int main(void) {
    seq_model_track_t track;
    seq_live_capture_t capture;
    seq_live_capture_plan_t plan;

    seq_plock_pool_reset();
    seq_model_track_init(&track);

    seq_live_capture_config_t cfg = {
        .track = &track,
    };
    seq_live_capture_init(&capture, &cfg);
    seq_live_capture_set_recording(&capture, true);

    clock_step_info_t clock = make_clock(0U, 0U);
    seq_live_capture_update_clock(&capture, &clock);

    seq_live_capture_input_t note_on = {
        .type = SEQ_LIVE_CAPTURE_EVENT_NOTE_ON,
        .note = 60U,
        .velocity = 100U,
        .voice_index = 0U,
        .timestamp = 0U,
    };

    assert(seq_live_capture_plan_event(&capture, &note_on, &plan));
    const size_t primary_step = plan.step_index;
    const uint16_t used_before = seq_plock_pool_used();
    assert(seq_live_capture_commit_plan(&capture, &plan));
    const uint16_t used_after_first = seq_plock_pool_used();
    if ((uint16_t)(used_after_first - used_before) != 3U) {
        fprintf(stderr, "unexpected pool usage after first commit: before=%u after=%u\n",
                used_before,
                used_after_first);
        return 1;
    }

    seq_live_capture_input_t note_on_update = note_on;
    note_on_update.velocity = 45U;
    note_on_update.timestamp = 2U;
    assert(seq_live_capture_plan_event(&capture, &note_on_update, &plan));
    assert(seq_live_capture_commit_plan(&capture, &plan));
    if (seq_plock_pool_used() != used_after_first) {
        fprintf(stderr, "pool usage changed on dedup update (%u -> %u)\n",
                used_after_first,
                seq_plock_pool_used());
        return 1;
    }

    const seq_model_step_t *step0 = &track.steps[primary_step % SEQ_MODEL_STEPS_PER_TRACK];
    if (!step_has_velocity(step0, note_on_update.velocity)) {
        fprintf(stderr, "last-wins velocity mismatch\n");
        return 1;
    }

    clock = make_clock(1U, clock.now + clock.step_st);
    seq_live_capture_update_clock(&capture, &clock);
    seq_live_capture_input_t note_off = {
        .type = SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF,
        .note = note_on.note,
        .velocity = 0U,
        .voice_index = 0U,
        .timestamp = clock.now,
    };
    assert(seq_live_capture_plan_event(&capture, &note_off, &plan));
    const uint16_t used_before_off = seq_plock_pool_used();
    assert(seq_live_capture_commit_plan(&capture, &plan));
    const uint16_t used_after_off = seq_plock_pool_used();
    if (used_after_off <= used_after_first) {
        fprintf(stderr, "pool usage did not grow on length commit (%u -> %u)\n",
                used_before_off,
                used_after_off);
        return 1;
    }
    if (capture_has_active_voice(&capture)) {
        fprintf(stderr, "voice tracker left active after NOTE_OFF\n");
        return 1;
    }

    clock = make_clock(2U, clock.now + clock.step_st);
    seq_live_capture_update_clock(&capture, &clock);
    seq_live_capture_input_t note_on_fail = {
        .type = SEQ_LIVE_CAPTURE_EVENT_NOTE_ON,
        .note = 62U,
        .velocity = 110U,
        .voice_index = 0U,
        .timestamp = clock.now,
    };
    assert(seq_live_capture_plan_event(&capture, &note_on_fail, &plan));
    const size_t failing_step = plan.step_index % SEQ_MODEL_STEPS_PER_TRACK;
    const uint16_t used_before_fail = seq_plock_pool_used();
    if (seq_live_capture_commit_plan(&capture, &plan)) {
        fprintf(stderr, "expected commit failure when pool is exhausted\n");
        return 1;
    }
    if (seq_plock_pool_used() != used_before_fail) {
        fprintf(stderr, "pool usage changed despite rollback (%u -> %u)\n",
                used_before_fail,
                seq_plock_pool_used());
        return 1;
    }
    if (seq_model_step_plock_count(&track.steps[failing_step]) != 0U) {
        fprintf(stderr, "step %zu retains partial state after rollback\n", failing_step);
        return 1;
    }
    if (capture_has_active_voice(&capture)) {
        fprintf(stderr, "voice tracker left active after rollback\n");
        return 1;
    }

    printf("Live Rec sanity OK (pool used %u -> %u -> %u, rollback preserved)\n",
           used_before,
           used_after_first,
           used_after_off);
    return 0;
}
