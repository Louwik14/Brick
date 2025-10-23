/**
 * @file seq_model_tests.c
 * @brief Host-side tests for the Brick sequencer model helpers.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "core/seq/seq_model.h"

static void test_generation_helpers(void) {
    seq_model_gen_t gen_a;
    seq_model_gen_t gen_b;

    gen_a.value = 42U;
    seq_model_gen_reset(&gen_a);
    assert(gen_a.value == 0U);

    seq_model_gen_reset(&gen_b);
    assert(!seq_model_gen_has_changed(&gen_a, &gen_b));

    seq_model_gen_bump(&gen_a);
    assert(seq_model_gen_has_changed(&gen_a, &gen_b));
}

static void test_default_step_initialisation(void) {
    seq_model_step_t step;
    size_t i;

    seq_model_step_init(&step);
#if !SEQ_FEATURE_PLOCK_POOL
    assert(step.plock_count == 0U);
#endif
    assert(!seq_model_step_has_playable_voice(&step));
    assert(!seq_model_step_is_automation_only(&step));

    for (i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        const seq_model_voice_t *voice = seq_model_step_get_voice(&step, i);
        assert(voice != NULL);

        assert(voice->state == SEQ_MODEL_VOICE_DISABLED);
        if (i == 0U) {
            assert(voice->velocity == SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY);
        } else {
            assert(voice->velocity == SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY);
        }

        assert(voice->note == 60U);
        assert(voice->length == 16U);
        assert(voice->micro_offset == 0);
    }

    const seq_model_step_offsets_t *offsets = seq_model_step_get_offsets(&step);
    assert(offsets != NULL);
    assert(offsets->transpose == 0);
    assert(offsets->velocity == 0);
    assert(offsets->length == 0);
    assert(offsets->micro == 0);
}

static void test_step_state_helpers(void) {
    seq_model_step_t step;
    seq_model_step_init(&step);

    assert(!seq_model_step_has_playable_voice(&step));
    assert(!seq_model_step_has_any_plock(&step));
    assert(!seq_model_step_is_automation_only(&step));

    seq_model_step_make_neutral(&step);
    assert(seq_model_step_has_playable_voice(&step));
    assert(!seq_model_step_is_automation_only(&step));

    const seq_model_voice_t *primary = seq_model_step_get_voice(&step, 0U);
    assert(primary != NULL);
    assert(primary->velocity == SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY);
    assert(primary->length == 1U);
    assert(primary->micro_offset == 0);

    for (size_t v = 1U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        const seq_model_voice_t *voice = seq_model_step_get_voice(&step, v);
        assert(voice != NULL);
        assert(voice->velocity == 0U);
        assert(voice->length == 1U);
    }

    seq_model_step_make_automation_only(&step);
    assert(!seq_model_step_has_playable_voice(&step));
    assert(!seq_model_step_is_automation_only(&step));
    assert(!seq_model_step_has_seq_plock(&step));
    assert(!seq_model_step_has_cart_plock(&step));

    seq_model_plock_t plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = 64,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    assert(seq_model_step_add_plock(&step, &plock));
    assert(seq_model_step_has_any_plock(&step));
    assert(seq_model_step_has_seq_plock(&step));
    assert(!seq_model_step_has_cart_plock(&step));
    assert(!seq_model_step_is_automation_only(&step));

    seq_model_plock_t cart = {
        .domain = SEQ_MODEL_PLOCK_CART,
        .voice_index = 0U,
        .parameter_id = 1U,
        .value = 32,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    assert(seq_model_step_add_plock(&step, &cart));
    assert(seq_model_step_has_cart_plock(&step));
    assert(!seq_model_step_is_automation_only(&step));

    seq_model_step_init(&step);
    seq_model_step_make_automation_only(&step);
    assert(seq_model_step_add_plock(&step, &cart));
    assert(!seq_model_step_has_seq_plock(&step));
    assert(seq_model_step_has_cart_plock(&step));
    assert(seq_model_step_is_automation_only(&step));
}

static void test_plock_capacity_guard(void) {
    seq_model_step_t step;
    seq_model_plock_t plock;
    size_t i;

    seq_model_step_init(&step);
    memset(&plock, 0, sizeof(plock));
    plock.domain = SEQ_MODEL_PLOCK_INTERNAL;

    for (i = 0U; i < SEQ_MODEL_MAX_PLOCKS_PER_STEP; ++i) {
        plock.voice_index = 0U;
        assert(seq_model_step_add_plock(&step, &plock));
    }

#if !SEQ_FEATURE_PLOCK_POOL
    assert(step.plock_count == SEQ_MODEL_MAX_PLOCKS_PER_STEP);
#endif

    /* The next addition should be rejected because the buffer is full. */
    assert(!seq_model_step_add_plock(&step, &plock));
}

static void test_track_config_mutations(void) {
    seq_model_track_t track;
    seq_model_quantize_config_t quantize = {
        .enabled = true,
        .grid = SEQ_MODEL_QUANTIZE_1_32,
        .strength = 75U,
    };
    seq_model_transpose_config_t transpose = {
        .global = -2,
        .per_voice = { 0, 1, -1, 7 },
    };
    seq_model_scale_config_t scale = {
        .enabled = true,
        .root = 5U,
        .mode = SEQ_MODEL_SCALE_MINOR,
    };

    seq_model_track_init(&track);
    seq_model_track_set_quantize(&track, &quantize);
    seq_model_track_set_transpose(&track, &transpose);
    seq_model_track_set_scale(&track, &scale);

    assert(track.config.quantize.enabled == quantize.enabled);
    assert(track.config.quantize.grid == quantize.grid);
    assert(track.config.quantize.strength == quantize.strength);

    assert(track.config.transpose.global == transpose.global);
    for (size_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        assert(track.config.transpose.per_voice[i] == transpose.per_voice[i]);
    }

    assert(track.config.scale.enabled == scale.enabled);
    assert(track.config.scale.root == scale.root);
    assert(track.config.scale.mode == scale.mode);
}

int main(void) {
    test_generation_helpers();
    test_default_step_initialisation();
    test_step_state_helpers();
    test_plock_capacity_guard();
    test_track_config_mutations();

    printf("seq_model_tests: OK\n");
    return 0;
}
