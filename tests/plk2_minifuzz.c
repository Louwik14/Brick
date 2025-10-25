#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_plock_pool.h"

static uint32_t s_rng = 0x12345678U;

static uint32_t next_rand(void) {
    s_rng = (1103515245U * s_rng + 12345U);
    return s_rng;
}

static uint8_t rand_u8(void) {
    return (uint8_t)(next_rand() >> 24);
}

int main(void) {
    enum { kIterations = 2000 };
    uint32_t ok_count = 0U;
    uint32_t truncated_count = 0U;
    uint32_t invalid_count = 0U;
    uint32_t missing_count = 0U;

    for (int iter = 0; iter < kIterations; ++iter) {
        uint8_t buffer[128] = {0};
        size_t total_len = 0U;
        uint32_t flags = next_rand();
        bool emit_tag = (flags & 1U) != 0U;
        bool provide_count = (flags & 2U) != 0U;
        bool force_truncate = (flags & 4U) != 0U;
        uint8_t stored_count = (uint8_t)(next_rand() % 32U);
        size_t payload_len = (size_t)stored_count * 3U;
        size_t provided_payload = payload_len;

        if (emit_tag) {
            buffer[0] = 'P';
            buffer[1] = 'L';
            buffer[2] = 'K';
            buffer[3] = '2';
            total_len = 4U;
            if (provide_count) {
                buffer[4] = stored_count;
                total_len = 5U;
                if (force_truncate && (payload_len > 0U)) {
                    provided_payload = (size_t)(next_rand() % payload_len);
                }
                if ((5U + provided_payload) > sizeof(buffer)) {
                    provided_payload = sizeof(buffer) - 5U;
                }
                for (size_t i = 0U; i < provided_payload; ++i) {
                    buffer[5U + i] = rand_u8();
                }
                total_len += provided_payload;
            } else {
                /* Missing count: truncate chunk immediately after tag. */
                total_len = 4U;
            }
        } else {
            total_len = (size_t)(next_rand() % sizeof(buffer));
            for (size_t i = 0U; i < total_len; ++i) {
                buffer[i] = (uint8_t)('A' + (rand_u8() % 26U));
            }
            if (total_len >= 4U) {
                buffer[0] = 'B';
                buffer[1] = 'R';
                buffer[2] = 'K';
                buffer[3] = '!';
            }
        }

        if (emit_tag && provide_count && (provided_payload >= payload_len)) {
            for (uint8_t i = 0U; i < stored_count; ++i) {
                const size_t base = 5U + (size_t)i * 3U;
                if ((base + 2U) >= sizeof(buffer)) {
                    break;
                }
                buffer[base] = (uint8_t)(i + 1U);
                buffer[base + 1U] = (uint8_t)(255U - i);
                buffer[base + 2U] = (uint8_t)((i << 2U) & 0xFCU);
            }
        }

        seq_plock_pool_reset();
        seq_model_track_t track;
        memset(&track, 0, sizeof(track));

        bool decoded = seq_project_track_steps_decode(&track,
                                                       buffer,
                                                       total_len,
                                                       SEQ_PROJECT_PATTERN_VERSION,
                                                       SEQ_PROJECT_TRACK_DECODE_FULL);
        assert(decoded);

        const seq_model_step_t *step0 = &track.steps[0];
        const uint8_t actual_count = seq_model_step_plock_count(step0);

        const bool valid_payload = emit_tag && provide_count &&
                                   (stored_count <= SEQ_MAX_PLOCKS_PER_STEP) &&
                                   (provided_payload >= payload_len);

        if (!emit_tag || !provide_count) {
            if (actual_count != 0U) {
                fprintf(stderr, "iteration %d: expected empty step for missing chunk\n", iter);
                return 1;
            }
            missing_count++;
            continue;
        }

        if (!valid_payload) {
            if (actual_count != 0U) {
                fprintf(stderr, "iteration %d: decoder should have dropped invalid chunk (count=%u)\n",
                        iter,
                        stored_count);
                return 1;
            }
            if ((stored_count > SEQ_MAX_PLOCKS_PER_STEP)) {
                invalid_count++;
            } else {
                truncated_count++;
            }
            continue;
        }

        if (actual_count != stored_count) {
            fprintf(stderr, "iteration %d: expected %u entries, got %u\n",
                    iter,
                    stored_count,
                    actual_count);
            return 1;
        }

        for (uint8_t i = 0U; i < actual_count; ++i) {
            const plk2_t *entry = seq_model_step_get_plock(step0, i);
            const size_t base = 5U + (size_t)i * 3U;
            assert(entry != NULL);
            if ((base + 2U) >= sizeof(buffer)) {
                fprintf(stderr, "iteration %d: buffer underflow while checking entry %u\n", iter, i);
                return 1;
            }
            if ((entry->param_id != buffer[base]) ||
                (entry->value != buffer[base + 1U]) ||
                (entry->flags != buffer[base + 2U])) {
                fprintf(stderr, "iteration %d: entry mismatch at index %u\n", iter, i);
                return 1;
            }
        }
        ok_count++;
    }

    printf("PLK2 mini-fuzz OK: valid=%u truncated=%u invalid=%u missing=%u\n",
           ok_count,
           truncated_count,
           invalid_count,
           missing_count);
    return 0;
}
