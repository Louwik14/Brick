#include <assert.h>
#include <stdint.h>

#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_plock_pool.h"

static void test_ui_helper_success(void) {
    seq_plock_pool_reset();

    seq_model_step_t step;
    seq_model_step_init(&step);

    const uint8_t ids[3] = {
        PL_INT_ALL_TRANSP,
        (uint8_t)(PL_INT_NOTE_V0 + 1U),
        0x45U,
    };
    const uint8_t values[3] = {
        pl_u8_from_s8(-5),
        64U,
        0x7FU,
    };
    const uint8_t flags[3] = {
        0x02U,
        (uint8_t)(1U << 2),
        0x01U,
    };

    assert(seq_model_step_set_plocks_pooled(&step, ids, values, flags, 3U) == 0);
    assert(step.pl_ref.count == 3U);

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 1);

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flag = 0U;

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == ids[0] && value == values[0] && flag == flags[0]);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == ids[1] && value == values[1] && flag == flags[1]);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == ids[2] && value == values[2] && flag == flags[2]);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 0);
}

static void test_live_capture_helper_success(void) {
    seq_plock_pool_reset();

    seq_model_step_t step;
    seq_model_step_init(&step);

    const uint8_t ids[2] = {
        (uint8_t)(PL_INT_NOTE_V0 + 2U),
        (uint8_t)(PL_INT_MIC_V0 + 2U),
    };
    const uint8_t values[2] = {
        90U,
        pl_u8_from_s8(3),
    };
    const uint8_t flags[2] = {
        (uint8_t)(2U << 2),
        (uint8_t)((2U << 2) | 0x02U),
    };

    assert(seq_model_step_set_plocks_pooled(&step, ids, values, flags, 2U) == 0);
    assert(step.pl_ref.count == 2U);

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 1);

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flag = 0U;

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == ids[0] && value == values[0] && flag == flags[0]);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == ids[1] && value == values[1] && flag == flags[1]);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 0);
}

static void test_helper_oom_fallback(void) {
#if !SEQ_FEATURE_PLOCK_POOL
    seq_plock_pool_reset();

    seq_model_step_t step;
    seq_model_step_init(&step);

    seq_model_plock_t note_plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = 64,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    seq_model_plock_t velocity_plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = 90,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_VELOCITY,
    };

    assert(seq_model_step_add_plock(&step, &note_plock));
    assert(seq_model_step_add_plock(&step, &velocity_plock));

    const uint8_t requested = (uint8_t)(SEQ_PLOCK_POOL_CAPACITY_TEST + 1U);
    uint8_t ids[SEQ_PLOCK_POOL_CAPACITY_TEST + 1U];
    uint8_t values[SEQ_PLOCK_POOL_CAPACITY_TEST + 1U];
    uint8_t flags[SEQ_PLOCK_POOL_CAPACITY_TEST + 1U];

    for (uint16_t i = 0U; i < (uint16_t)requested; ++i) {
        ids[i] = (uint8_t)(0x40U + (i & 0x1FU));
        values[i] = (uint8_t)(i & 0xFFU);
        flags[i] = 0x01U;
    }

    assert(seq_model_step_set_plocks_pooled(&step, ids, values, flags, requested) == -1);
    assert(step.pl_ref.count == 0U);

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 1);

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flag = 0U;

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == (uint8_t)(PL_INT_NOTE_V0 + 0U));
    assert(value == 64U);

    assert(seq_reader_pl_next(&it, &id, &value, &flag) == 1);
    assert(id == (uint8_t)(PL_INT_VEL_V0 + 0U));
    assert(value == 90U);
#else
    /* Legacy per-step storage not available when pooled mode is enabled. */
    seq_plock_pool_reset();
#endif
}

int main(void) {
    test_ui_helper_success();
    test_live_capture_helper_success();
    test_helper_oom_fallback();
    return 0;
}
