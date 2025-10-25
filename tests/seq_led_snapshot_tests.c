#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef SEQ_USE_HANDLES
#define SEQ_USE_HANDLES 1
#endif

#include "core/seq/seq_access.h"
#include "core/seq/seq_plock_pool.h"

static void seed_test_pattern(void) {
    seq_runtime_init();
    seq_plock_pool_reset();

    seq_project_t *project = seq_runtime_access_project_mut();
    assert(project != NULL);
    assert(seq_project_set_active_slot(project, 0U, 0U));
    assert(seq_project_set_active_track(project, 0U));

    seq_model_track_t *track = seq_runtime_access_track_mut(0U);
    assert(track != NULL);

    for (size_t i = 0; i < SEQ_MODEL_STEPS_PER_TRACK; ++i) {
        seq_model_step_init(&track->steps[i]);
    }

    seq_model_step_t *step0 = &track->steps[0];
    seq_model_voice_t voice;
    seq_model_voice_init(&voice, true);
    voice.note = 64U;
    voice.velocity = 100U;
    voice.length = 12U;
    voice.micro_offset = -2;
    voice.state = SEQ_MODEL_VOICE_ENABLED;
    assert(seq_model_step_set_voice(step0, 0U, &voice));

    seq_model_step_t *step1 = &track->steps[1];
    seq_model_step_make_automation_only(step1);
    const plk2_t cart_plock = {
        .param_id = 0x41U,
        .value = 7U,
        .flags = 0x01U,
    };
    assert(seq_model_step_set_plocks_pooled(step1, &cart_plock, 1U) == 0);
}

static void render_led_frame(uint8_t *dst, size_t n_steps) {
    if ((dst == NULL) || (n_steps == 0U)) {
        return;
    }

    const seq_track_handle_t handle = seq_reader_get_active_track_handle();
    for (size_t i = 0; i < n_steps; ++i) {
        seq_step_view_t view;
        uint8_t cell = 0U;
        if (seq_reader_get_step(handle, (uint8_t)i, &view)) {
            if ((view.flags & SEQ_STEPF_AUTOMATION_ONLY) != 0U) {
                cell = 2U;
            } else if ((view.flags & SEQ_STEPF_HAS_VOICE) != 0U) {
                cell = 1U;
            }
            if ((view.flags & SEQ_STEPF_MUTED) != 0U) {
                cell |= 0x80U;
            }
        }
        dst[i] = cell;
    }
}

int main(void) {
    enum { kStepCount = SEQ_MODEL_STEPS_PER_TRACK };

    seed_test_pattern();

    uint8_t frame[kStepCount];
    memset(frame, 0, sizeof(frame));
    render_led_frame(frame, kStepCount);

    static const uint8_t k_reference[kStepCount] = {
        1U, 2U,
    };

    size_t diffs = 0U;
    for (size_t i = 0; i < kStepCount; ++i) {
        if (frame[i] != k_reference[i]) {
            printf(" mismatch[%zu]=%u ref=%u\n", i, frame[i], k_reference[i]);
            diffs++;
        }
    }

    printf("LED snapshot diffs: %zu\n", diffs);
    return 0;
}
