#include <assert.h>
#include <stdint.h>

#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_plock_pool.h"

static void test_open_empty_step(void) {
    seq_model_step_t step;
    seq_model_step_init(&step);

    seq_reader_pl_it_t it;
    assert(seq_reader_pl_open(&it, &step) == 0);
    uint8_t id = 0U, value = 0U, flags = 0U;
    assert(seq_reader_pl_next(&it, &id, &value, &flags) == 0);
}

static void test_pool_iter(void) {
    seq_plock_pool_reset();

    uint16_t offset = 0U;
    assert(seq_plock_pool_alloc(3U, &offset) == 0);

    seq_plock_entry_t *entry0 = seq_plock_pool_get(offset, 0U);
    seq_plock_entry_t *entry1 = seq_plock_pool_get(offset, 1U);
    seq_plock_entry_t *entry2 = seq_plock_pool_get(offset, 2U);

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

int main(void) {
    test_open_empty_step();
    test_pool_iter();
    return 0;
}
