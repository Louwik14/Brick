#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/seq/seq_model.h"
#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_plock_pool.h"
#include "core/seq/seq_plock_ids.h"

static void populate_track(seq_model_track_t *track) {
    seq_model_track_init(track);

    seq_model_step_t *step1 = &track->steps[1];
    const plk2_t single_entry[1] = {
        {
            .param_id = PL_INT_NOTE_V0,
            .value = 0U,
            .flags = 0U,
        },
    };
    assert(seq_model_step_set_plocks_pooled(step1, single_entry, 1U) == 0);

    seq_model_step_t *step2 = &track->steps[2];
    plk2_t packed[SEQ_MAX_PLOCKS_PER_STEP];
    for (uint8_t i = 0U; i < SEQ_MAX_PLOCKS_PER_STEP; ++i) {
        const bool cart_domain = (i & 0x01U) != 0U;
        packed[i].param_id = (uint8_t)(cart_domain ? (0x40U + i) : (PL_INT_VEL_V0 + (i & 0x03U)));
        packed[i].value = (uint8_t)((i & 0x01U) != 0U ? 127U : 0U);
        packed[i].flags = (uint8_t)((i & 0x03U) << SEQ_READER_PL_FLAG_VOICE_SHIFT);
        if (!cart_domain) {
            packed[i].flags |= SEQ_READER_PL_FLAG_SIGNED;
        } else {
            packed[i].flags |= SEQ_READER_PL_FLAG_DOMAIN_CART;
        }
    }
    assert(seq_model_step_set_plocks_pooled(step2, packed, SEQ_MAX_PLOCKS_PER_STEP) == 0);
}

static bool buffers_match(const uint8_t *a, size_t len_a, const uint8_t *b, size_t len_b) {
    return (len_a == len_b) && (memcmp(a, b, len_a) == 0);
}

int main(void) {
    uint8_t buffer_a[SEQ_PROJECT_PATTERN_STORAGE_MAX];
    uint8_t buffer_b[SEQ_PROJECT_PATTERN_STORAGE_MAX];
    size_t written_a = 0U;
    size_t written_b = 0U;

    seq_model_track_t original;
    seq_model_track_t decoded;

    seq_plock_pool_reset();
    populate_track(&original);

    assert(seq_project_track_steps_encode(&original, buffer_a, sizeof(buffer_a), &written_a));
    assert(written_a > 0U);

    seq_plock_pool_reset();
    assert(seq_project_track_steps_decode(&decoded,
                                          buffer_a,
                                          written_a,
                                          SEQ_PROJECT_PATTERN_VERSION,
                                          SEQ_PROJECT_TRACK_DECODE_FULL));

    assert(seq_project_track_steps_encode(&decoded, buffer_b, sizeof(buffer_b), &written_b));
    if (!buffers_match(buffer_a, written_a, buffer_b, written_b)) {
        fprintf(stderr, "mismatch after roundtrip (lenA=%zu lenB=%zu)\n", written_a, written_b);
        return 1;
    }

    printf("PLK2 roundtrip OK (%zu bytes)\n", written_a);
    return 0;
}
