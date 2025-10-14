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
    assert(step.plock_count == 0U);
    assert(!seq_model_step_has_active_voice(&step));
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

    assert(step.plock_count == SEQ_MODEL_MAX_PLOCKS_PER_STEP);

    /* The next addition should be rejected because the buffer is full. */
    assert(!seq_model_step_add_plock(&step, &plock));
}

static void test_pattern_config_mutations(void) {
    seq_model_pattern_t pattern;
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

    seq_model_pattern_init(&pattern);
    seq_model_pattern_set_quantize(&pattern, &quantize);
    seq_model_pattern_set_transpose(&pattern, &transpose);
    seq_model_pattern_set_scale(&pattern, &scale);

    assert(pattern.config.quantize.enabled == quantize.enabled);
    assert(pattern.config.quantize.grid == quantize.grid);
    assert(pattern.config.quantize.strength == quantize.strength);

    assert(pattern.config.transpose.global == transpose.global);
    for (size_t i = 0U; i < SEQ_MODEL_VOICES_PER_STEP; ++i) {
        assert(pattern.config.transpose.per_voice[i] == transpose.per_voice[i]);
    }

    assert(pattern.config.scale.enabled == scale.enabled);
    assert(pattern.config.scale.root == scale.root);
    assert(pattern.config.scale.mode == scale.mode);
}

int main(void) {
    test_generation_helpers();
    test_default_step_initialisation();
    test_plock_capacity_guard();
    test_pattern_config_mutations();

    printf("seq_model_tests: OK\n");
    return 0;
}
