#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_plock_ids.h"
#include "core/seq/seq_plock_pool.h"
#include "core/seq/seq_project.h"

static size_t write_step_header(uint8_t *dst, uint8_t skip, uint8_t flags, uint8_t voice_mask, uint8_t plock_count) {
    dst[0] = skip;
    dst[1] = flags;
    dst[2] = voice_mask;
    dst[3] = plock_count;
    return 4U;
}

int main(void) {
    seq_plock_pool_reset();

    uint8_t buffer[128];
    uint8_t *cursor = buffer;

    const uint16_t step_count = 1U;
    memcpy(cursor, &step_count, sizeof(step_count));
    cursor += sizeof(step_count);

    cursor += write_step_header(cursor, 0U, 0U, 0U, 0U);

    const uint8_t chunk_tag[4] = {'P', 'L', 'K', '2'};
    memcpy(cursor, chunk_tag, sizeof(chunk_tag));
    cursor += sizeof(chunk_tag);

    const uint8_t count = 2U;
    *cursor++ = count;

    const uint8_t ids[2] = {PL_INT_ALL_VEL, 0x40U};
    const uint8_t values[2] = {pl_u8_from_s8(4), 0x7FU};
    const uint8_t flags[2] = {0x00U, 0x01U};
    for (uint8_t i = 0U; i < count; ++i) {
        *cursor++ = ids[i];
        *cursor++ = values[i];
        *cursor++ = flags[i];
    }

    const size_t payload_len = (size_t)(cursor - buffer);

    seq_model_track_t track;
    assert(seq_project_track_steps_decode(&track,
                                          buffer,
                                          payload_len,
                                          2U,
                                          SEQ_PROJECT_TRACK_DECODE_FULL));

    const seq_model_step_t *step = &track.steps[0];
    assert(step->pl_ref.count == count);

    for (uint8_t i = 0U; i < count; ++i) {
        const seq_plock_entry_t *entry = seq_plock_pool_get(step->pl_ref.offset, i);
        assert(entry != NULL);
        assert(entry->param_id == ids[i]);
        assert(entry->value == values[i]);
        assert(entry->flags == flags[i]);
    }

    return 0;
}
