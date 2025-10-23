#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"

static size_t write_step_header(uint8_t *dst, uint8_t skip, uint8_t flags, uint8_t voice_mask, uint8_t plock_count) {
    dst[0] = skip;
    dst[1] = flags;
    dst[2] = voice_mask;
    dst[3] = plock_count;
    return 4U;
}

int main(void) {
    uint8_t buffer[128];
    uint8_t *cursor = buffer;

    const uint16_t step_count = 1U;
    memcpy(cursor, &step_count, sizeof(step_count));
    cursor += sizeof(step_count);

    cursor += write_step_header(cursor, 0U, 0U, 0U, 0U);

    const uint8_t chunk_tag[4] = {'P', 'L', 'K', '2'};
    memcpy(cursor, chunk_tag, sizeof(chunk_tag));
    cursor += sizeof(chunk_tag);

    const uint8_t count = 3U;
    *cursor++ = count;

    /* Only 7 bytes instead of 9 -> truncated payload */
    for (uint8_t i = 0U; i < 7U; ++i) {
        *cursor++ = (uint8_t)(0x20U + i);
    }

    const size_t payload_len = (size_t)(cursor - buffer);

    seq_model_track_t track;
    assert(seq_project_track_steps_decode(&track,
                                          buffer,
                                          payload_len,
                                          2U,
                                          SEQ_PROJECT_TRACK_DECODE_FULL));

    const seq_model_step_t *step = &track.steps[0];
    assert(step->pl_ref.count == 0U);

    return 0;
}
