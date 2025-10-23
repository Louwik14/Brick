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

    const uint8_t voice_mask = 0x01U;
    const uint8_t payload_mask = 0x01U; /* voice 0 payload */
    const uint8_t flags = (uint8_t)(payload_mask << 3);
    cursor += write_step_header(cursor, 0U, flags, voice_mask, 1U);

    const uint8_t note = 64U;
    const uint8_t velocity = 96U;
    const uint8_t length = 12U;
    const int8_t micro = 0;
    *cursor++ = note;
    *cursor++ = velocity;
    *cursor++ = length;
    *cursor++ = (uint8_t)micro;

    const int16_t pl_value = 111;
    memcpy(cursor, &pl_value, sizeof(pl_value));
    cursor += sizeof(pl_value);
    const uint8_t meta = 0x00U;
    *cursor++ = meta;

    const size_t payload_len = (size_t)(cursor - buffer);

    seq_model_track_t track;
    assert(seq_project_track_steps_decode(&track,
                                          buffer,
                                          payload_len,
                                          2U,
                                          SEQ_PROJECT_TRACK_DECODE_FULL));

    const seq_model_step_t *step = &track.steps[0];
    assert(step->pl_ref.count == 0U);
    const seq_model_voice_t *voice = &step->voices[0];
    assert(voice->state == SEQ_MODEL_VOICE_ENABLED);
    assert(voice->note == note);
    assert(voice->velocity == velocity);
    assert(voice->length == length);
    assert(voice->micro_offset == micro);

    return 0;
}
