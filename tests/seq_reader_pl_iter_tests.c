#include <assert.h>
#include <stdint.h>

#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_pool.h"
#endif

static void test_open_empty_step(void) {
    seq_model_step_t step;
    seq_model_step_init(&step);

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 0);
    uint8_t id = 0U, value = 0U, flags = 0U;
    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 0);
}

static void test_legacy_iter(void) {
    seq_model_step_t step;
    seq_model_step_init(&step);

    seq_model_plock_t note_plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 2U,
        .parameter_id = 0U,
        .value = 64,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    assert(seq_model_step_add_plock(&step, &note_plock));

    seq_model_plock_t transpose_plock = {
        .domain = SEQ_MODEL_PLOCK_INTERNAL,
        .voice_index = 0U,
        .parameter_id = 0U,
        .value = -5,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR,
    };
    assert(seq_model_step_add_plock(&step, &transpose_plock));

    seq_model_plock_t cart_plock = {
        .domain = SEQ_MODEL_PLOCK_CART,
        .voice_index = 1U,
        .parameter_id = 0x52U,
        .value = 99,
        .internal_param = SEQ_MODEL_PLOCK_PARAM_NOTE,
    };
    assert(seq_model_step_add_plock(&step, &cart_plock));

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 1);

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flags = 0U;

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == (uint8_t)(PL_INT_NOTE_V0 + 2U));
    assert(value == 64U);
    assert(flags == (uint8_t)(2U << 2));

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == PL_INT_ALL_TRANSP);
    assert(value == pl_u8_from_s8(-5));
    assert(flags == 0x02U);

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == 0x52U);
    assert(value == 99U);
    assert(flags == 0x01U);

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 0);
}

#if SEQ_FEATURE_PLOCK_POOL
static void test_pool_iter(void) {
    seq_plock_pool_reset();

    uint16_t offset = 0U;
    assert(seq_plock_pool_alloc(3U, &offset) == 0);

    seq_plock_entry_t *entry0 = (seq_plock_entry_t *)(uintptr_t)seq_plock_pool_get(offset, 0U);
    seq_plock_entry_t *entry1 = (seq_plock_entry_t *)(uintptr_t)seq_plock_pool_get(offset, 1U);
    seq_plock_entry_t *entry2 = (seq_plock_entry_t *)(uintptr_t)seq_plock_pool_get(offset, 2U);

    entry0->param_id = 0x10U;
    entry0->value = 0xAAU;
    entry0->flags = 0x01U;

    entry1->param_id = 0x20U;
    entry1->value = 0x55U;
    entry1->flags = 0x80U;

    entry2->param_id = 0x30U;
    entry2->value = 0x7FU;
    entry2->flags = 0x40U;

    seq_model_step_t step;
    seq_model_step_init(&step);
    step.pl_ref.offset = offset;
    step.pl_ref.count = 3U;

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 1);

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flags = 0U;

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == 0x10U && value == 0xAAU && flags == 0x01U);

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == 0x20U && value == 0x55U && flags == 0x80U);

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 1);
    assert(id == 0x30U && value == 0x7FU && flags == 0x40U);

    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 0);
}
#endif

int main(void) {
    test_open_empty_step();
    test_legacy_iter();
#if SEQ_FEATURE_PLOCK_POOL
    test_pool_iter();
#endif
    return 0;
}
