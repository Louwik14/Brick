/**
 * @file seq_runtime.c
 * @brief Runtime snapshot publication for the Brick sequencer.
 * @ingroup seq_runtime
 */

#include "seq_runtime.h"

#include <string.h>
#include <stdatomic.h>

static seq_runtime_t      s_buffers[2];
static atomic_uint_fast8_t s_active;

static inline uint8_t clamp_u8(int value, int lo, int hi) {
    if (value < lo) return (uint8_t)lo;
    if (value > hi) return (uint8_t)hi;
    return (uint8_t)value;
}

static inline int16_t clamp_s16(int value, int lo, int hi) {
    if (value < lo) return (int16_t)lo;
    if (value > hi) return (int16_t)hi;
    return (int16_t)value;
}

void seq_runtime_init(void) {
    memset(s_buffers, 0, sizeof(s_buffers));
    atomic_init(&s_active, 0);
}

void seq_runtime_snapshot_from_pattern(seq_runtime_t *dst,
                                       const seq_pattern_t *pattern,
                                       uint32_t playhead_index) {
    if (!dst || !pattern) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->generation = pattern->generation;
    dst->playhead   = playhead_index;
    dst->offsets    = pattern->offsets;

    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        const seq_track_t *voice = &pattern->voices[v];
        seq_runtime_voice_t *rv  = &dst->voices[v];
        uint16_t voice_length    = seq_model_voice_length(pattern, v);
        rv->length = voice_length;

        for (uint16_t s = 0; s < SEQ_MODEL_STEP_COUNT; ++s) {
            const seq_step_t *st = &voice->steps[s];
            seq_runtime_step_t *rst = &rv->steps[s];

            bool base_active = st->active && st->velocity > 0;
            bool plocked_note = (st->plock_mask & (seq_plock_mask_t)(1u << SEQ_PARAM_NOTE)) != 0;
            bool plocked_vel  = (st->plock_mask & (seq_plock_mask_t)(1u << SEQ_PARAM_VELOCITY)) != 0;
            bool plocked_len  = (st->plock_mask & (seq_plock_mask_t)(1u << SEQ_PARAM_LENGTH)) != 0;
            bool plocked_mic  = (st->plock_mask & (seq_plock_mask_t)(1u << SEQ_PARAM_MICRO_TIMING)) != 0;

            int note = plocked_note ? st->params[SEQ_PARAM_NOTE] : st->note;
            note += pattern->offsets.transpose;
            rst->note = clamp_u8(note, 0, 127);
            rst->params[SEQ_PARAM_NOTE] = rst->note;

            int velocity = base_active ? st->velocity : 0;
            if (plocked_vel) {
                velocity = st->params[SEQ_PARAM_VELOCITY];
            }
            velocity += pattern->offsets.velocity;
            velocity = clamp_s16(velocity, 0, 127);
            rst->velocity = (uint8_t)velocity;
            rst->params[SEQ_PARAM_VELOCITY] = velocity;

            int length = st->length;
            if (plocked_len) {
                length = st->params[SEQ_PARAM_LENGTH];
            }
            length += pattern->offsets.length;
            length = clamp_s16(length, 1, SEQ_MODEL_STEP_COUNT);
            rst->length = (uint8_t)length;
            rst->params[SEQ_PARAM_LENGTH] = length;

            int micro = st->micro_timing;
            if (plocked_mic) {
                micro = st->params[SEQ_PARAM_MICRO_TIMING];
            }
            micro += pattern->offsets.micro_timing;
            micro = clamp_s16(micro, -SEQ_MODEL_MICRO_OFFSET_RANGE, SEQ_MODEL_MICRO_OFFSET_RANGE);
            rst->micro_timing = (int8_t)micro;
            rst->params[SEQ_PARAM_MICRO_TIMING] = micro;

            rst->plock_mask = st->plock_mask;
            rst->active     = (rst->velocity > 0) && st->active;
            rst->param_only = (rst->velocity == 0) && (st->plock_mask != 0);
        }
    }
}

void seq_runtime_publish(const seq_runtime_t *snapshot) {
    if (!snapshot) {
        return;
    }
    uint8_t inactive = (uint8_t)((atomic_load_explicit(&s_active, memory_order_relaxed) ^ 1u) & 1u);
    s_buffers[inactive] = *snapshot;
    atomic_store_explicit(&s_active, inactive, memory_order_release);
}

const seq_runtime_t *seq_runtime_get_snapshot(void) {
    uint8_t idx = (uint8_t)(atomic_load_explicit(&s_active, memory_order_acquire) & 1u);
    return &s_buffers[idx];
}

static inline const seq_runtime_step_t *access_step(const seq_runtime_t *snapshot,
                                                    uint8_t voice,
                                                    uint32_t step_idx) {
    if (!snapshot || voice >= SEQ_MODEL_VOICE_COUNT) {
        return NULL;
    }
    const seq_runtime_voice_t *rv = &snapshot->voices[voice];
    uint16_t len = rv->length ? rv->length : SEQ_MODEL_STEP_COUNT;
    if (len == 0) {
        return NULL;
    }
    uint32_t local = step_idx % len;
    return &rv->steps[local];
}

bool seq_runtime_step_has_note(const seq_runtime_t *snapshot, uint32_t step_idx) {
    if (!snapshot) {
        return false;
    }
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        const seq_runtime_step_t *st = access_step(snapshot, v, step_idx);
        if (st && st->active) {
            return true;
        }
    }
    return false;
}

bool seq_runtime_step_param_is_plocked(const seq_runtime_t *snapshot,
                                       uint32_t step_idx,
                                       uint8_t param) {
    if (!snapshot || param >= SEQ_PARAM_COUNT) {
        return false;
    }
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        const seq_runtime_step_t *st = access_step(snapshot, v, step_idx);
        if (st && (st->plock_mask & (seq_plock_mask_t)(1u << param))) {
            return true;
        }
    }
    return false;
}

int16_t seq_runtime_step_param_value(const seq_runtime_t *snapshot,
                                     uint32_t step_idx,
                                     uint8_t param) {
    if (!snapshot || param >= SEQ_PARAM_COUNT) {
        return 0;
    }
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        const seq_runtime_step_t *st = access_step(snapshot, v, step_idx);
        if (st && (st->plock_mask & (seq_plock_mask_t)(1u << param))) {
            return st->params[param];
        }
    }
    /* Fallback: return the first voice value even if not plocked to keep UI coherent. */
    const seq_runtime_step_t *fallback = access_step(snapshot, 0, step_idx);
    return fallback ? fallback->params[param] : 0;
}

uint32_t seq_runtime_playhead_index(const seq_runtime_t *snapshot) {
    if (!snapshot) {
        return 0;
    }
    return snapshot->playhead;
}
