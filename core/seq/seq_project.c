/**
 * @file seq_project.c
 * @brief Sequencer multi-track project helpers implementation.
 */

#include "seq_project.h"

#include <string.h>

#include "brick_config.h"
#include "board/board_flash.h"
#include "cart/cart_registry.h"
#include "core/seq/runtime/seq_runtime_cold.h"
#include "core/ram_audit.h"
#if SEQ_FEATURE_PLOCK_POOL
#include "core/seq/seq_plock_pool.h"
#endif

#define SEQ_PROJECT_DIRECTORY_MAGIC 0x4250524FU /* 'BPRO' */
#define SEQ_PROJECT_PATTERN_MAGIC   0x42504154U /* 'BPAT' */
#define SEQ_PROJECT_DIRECTORY_VERSION 1U

typedef struct __attribute__((packed)) {
    uint32_t offset;      /**< Relative offset inside the project slot. */
    uint32_t length;      /**< Payload length in bytes. */
    uint8_t  version;     /**< On-disk pattern version. */
    uint8_t  track_count; /**< Tracks stored in the payload. */
    uint8_t  reserved[2]; /**< Reserved for future use. */
} seq_project_directory_entry_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;                           /**< Directory identifier. */
    uint16_t version;                         /**< Directory format version. */
    uint16_t project_index;                   /**< Slot index inside external flash. */
    uint32_t tempo;                           /**< Project tempo snapshot. */
    uint8_t  active_bank;                     /**< Active bank when saved. */
    uint8_t  active_pattern;                  /**< Active pattern when saved. */
    uint8_t  track_count;                     /**< Runtime track count when saved. */
    uint8_t  reserved;                        /**< Reserved for alignment. */
    char     name[SEQ_PROJECT_NAME_MAX];      /**< Project label. */
    seq_project_directory_entry_t entries[SEQ_PROJECT_BANK_COUNT][SEQ_PROJECT_PATTERNS_PER_BANK];
} seq_project_directory_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;     /**< Pattern blob identifier. */
    uint16_t version;   /**< Pattern blob version. */
    uint8_t  track_count; /**< Number of tracks stored in this blob. */
    uint8_t  reserved;  /**< Reserved for alignment. */
} pattern_blob_header_t;

typedef struct __attribute__((packed)) {
    uint32_t cart_id;        /**< Cartridge identifier. */
    uint32_t payload_size;   /**< Size of the serialized payload. */
    uint8_t  slot_id;        /**< Slot used during save. */
    uint8_t  flags;          /**< Track flags (muted...). */
    uint16_t capabilities;   /**< Capability mask. */
} track_payload_header_t;

typedef struct __attribute__((packed)) {
    uint8_t step_index;  /**< Step index inside the track. */
    uint8_t flags;       /**< Step flags bitmask. */
    uint8_t voice_mask;  /**< Mask of enabled voices. */
    uint8_t plock_count; /**< Number of serialized parameter locks. */
} track_step_v1_header_t;

typedef struct __attribute__((packed)) {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    int8_t  micro;
    uint8_t state;
} track_voice_v1_payload_t;

typedef struct __attribute__((packed)) {
    int16_t velocity;
    int8_t  transpose;
    int8_t  length;
    int8_t  micro;
} track_offsets_payload_t;

typedef struct __attribute__((packed)) {
    int16_t value;
    uint16_t parameter_id;
    uint8_t domain;
    uint8_t voice_index;
    uint8_t internal_param;
} track_plock_v1_payload_t;

typedef struct __attribute__((packed)) {
    uint8_t skip;        /**< Number of neutral steps before this entry. */
    uint8_t flags;       /**< Step flags bitmask. */
    uint8_t voice_mask;  /**< Mask of enabled voices. */
    uint8_t plock_count; /**< Number of serialized parameter locks. */
} track_step_v2_header_t;

typedef struct __attribute__((packed)) {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    int8_t  micro;
} track_voice_v2_payload_t;

typedef struct __attribute__((packed)) {
    int16_t value;
    uint8_t meta;
} track_plock_v2_payload_t;

enum {
    STEP_FLAG_ACTIVE     = 1U << 0,
    STEP_FLAG_AUTOMATION = 1U << 1,
    STEP_FLAG_OFFSETS    = 1U << 2
};

typedef enum {
    TRACK_LOAD_FULL = 0,
    TRACK_LOAD_REMAPPED,
    TRACK_LOAD_DIFFERENT_CART,
    TRACK_LOAD_ABSENT
} track_load_policy_t;

static seq_project_t *s_active_project;
static CCM_DATA uint8_t s_pattern_buffer[SEQ_PROJECT_PATTERN_STORAGE_MAX];
UI_RAM_AUDIT(s_pattern_buffer);

static void pattern_desc_reset(seq_project_pattern_desc_t *desc) {
    if (desc == NULL) {
        return;
    }
    memset(desc, 0, sizeof(*desc));
    desc->version = SEQ_PROJECT_PATTERN_VERSION;
}

static inline void project_bind(seq_project_t *project) {
    s_active_project = project;
}

static bool ensure_flash_ready(void) {
    if (board_flash_is_ready()) {
        return true;
    }
    return board_flash_init();
}

static uint32_t project_base(uint8_t project_index) {
    return (uint32_t)project_index * SEQ_PROJECT_FLASH_SLOT_SIZE;
}

static uint8_t pattern_linear_index(uint8_t bank, uint8_t pattern) {
    return (uint8_t)(bank * SEQ_PROJECT_PATTERNS_PER_BANK + pattern);
}

static uint32_t pattern_offset(uint8_t project_index, uint8_t bank, uint8_t pattern) {
    return project_base(project_index) + (uint32_t)sizeof(seq_project_directory_t) +
           (uint32_t)pattern_linear_index(bank, pattern) * (uint32_t)SEQ_PROJECT_PATTERN_STORAGE_MAX;
}

static bool offsets_is_zero(const seq_model_step_offsets_t *offsets) {
    return (offsets->velocity == 0) && (offsets->transpose == 0) &&
           (offsets->length == 0) && (offsets->micro == 0);
}

static bool voice_equals_default(const seq_model_voice_t *voice, bool primary) {
    seq_model_voice_t ref;
    seq_model_voice_init(&ref, primary);
    return (voice->state == ref.state) && (voice->note == ref.note) &&
           (voice->velocity == ref.velocity) && (voice->length == ref.length) &&
           (voice->micro_offset == ref.micro_offset);
}

static bool step_needs_persist(const seq_model_step_t *step) {
    if (step->flags.active || step->flags.automation) {
        return true;
    }
#if !SEQ_FEATURE_PLOCK_POOL
    if (step->plock_count > 0U) {
        return true;
    }
#endif
    if (!offsets_is_zero(&step->offsets)) {
        return true;
    }
    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        if (!voice_equals_default(&step->voices[v], v == 0U)) {
            return true;
        }
    }
    return false;
}

#if BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
static uint8_t compute_voice_payload_mask(const seq_model_step_t *step) {
    uint8_t mask = 0U;
    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        seq_model_voice_t ref;
        seq_model_voice_init(&ref, v == 0U);
        const seq_model_voice_t *voice = &step->voices[v];
        if ((voice->note != ref.note) || (voice->velocity != ref.velocity) ||
            (voice->length != ref.length) || (voice->micro_offset != ref.micro_offset)) {
            mask |= (uint8_t)(1U << v);
        }
    }
    return mask;
}
#endif

static bool buffer_write(uint8_t **cursor, size_t *remaining, const void *src, size_t len) {
    if (*remaining < len) {
        return false;
    }
    memcpy(*cursor, src, len);
    *cursor += len;
    *remaining -= len;
    return true;
}

#if SEQ_FEATURE_PLOCK_POOL
static bool encode_plk2_chunk(const seq_model_step_t *step,
                              uint8_t **cursor,
                              size_t *remaining,
                              bool enable_plk2) {
    if (!enable_plk2) {
        return true;
    }

    if ((step == NULL) || (step->pl_ref.count == 0U)) {
        return true;
    }

    const uint8_t chunk_tag[4] = {'P', 'L', 'K', '2'};
    const uint8_t count = step->pl_ref.count;
    const size_t payload_len = (size_t)count * 3U;
    const size_t chunk_len = sizeof(chunk_tag) + sizeof(count) + payload_len;

    if (*remaining < chunk_len) {
        return false;
    }

    memcpy(*cursor, chunk_tag, sizeof(chunk_tag));
    *cursor += sizeof(chunk_tag);
    *remaining -= sizeof(chunk_tag);

    **cursor = count;
    *cursor += sizeof(count);
    *remaining -= sizeof(count);

    uint8_t *dst = *cursor;
    for (uint8_t i = 0U; i < count; ++i) {
        const seq_plock_entry_t *entry = seq_plock_pool_get(step->pl_ref.offset, i);
        if (entry == NULL) {
            return false;
        }
        *dst++ = entry->param_id;
        *dst++ = entry->value;
        *dst++ = entry->flags;
    }

    *cursor = dst;
    *remaining -= payload_len;
    return true;
}
#endif

#if !BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
static bool encode_track_steps_v1(const seq_model_track_t *track,
                                  uint8_t **cursor,
                                  size_t *remaining,
                                  bool write_plk2) {
#if !SEQ_FEATURE_PLOCK_POOL
    (void)write_plk2;
#endif
    uint16_t step_count = 0U;
    uint8_t *count_ptr = *cursor;
    if (!buffer_write(cursor, remaining, &step_count, sizeof(step_count))) {
        return false;
    }

    for (uint8_t i = 0U; i < SEQ_MODEL_STEPS_PER_TRACK; ++i) {
        const seq_model_step_t *step = &track->steps[i];
        if (!step_needs_persist(step)) {
            continue;
        }

        track_step_v1_header_t header;
        header.step_index = i;
        header.flags = 0U;
        header.voice_mask = 0U;
#if !SEQ_FEATURE_PLOCK_POOL
        header.plock_count = step->plock_count;
#else
        header.plock_count = 0U;
#endif

        if (step->flags.active) {
            header.flags |= STEP_FLAG_ACTIVE;
        }
        if (step->flags.automation) {
            header.flags |= STEP_FLAG_AUTOMATION;
        }
        if (!offsets_is_zero(&step->offsets)) {
            header.flags |= STEP_FLAG_OFFSETS;
        }
        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if (step->voices[v].state == SEQ_MODEL_VOICE_ENABLED) {
                header.voice_mask |= (uint8_t)(1U << v);
            }
        }

        if (!buffer_write(cursor, remaining, &header, sizeof(header))) {
            return false;
        }

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            track_voice_v1_payload_t payload;
            const seq_model_voice_t *voice = &step->voices[v];
            payload.note = voice->note;
            payload.velocity = voice->velocity;
            payload.length = voice->length;
            payload.micro = voice->micro_offset;
            payload.state = voice->state;
            if (!buffer_write(cursor, remaining, &payload, sizeof(payload))) {
                return false;
            }
        }

        if ((header.flags & STEP_FLAG_OFFSETS) != 0U) {
            track_offsets_payload_t offsets;
            offsets.velocity = step->offsets.velocity;
            offsets.transpose = step->offsets.transpose;
            offsets.length = step->offsets.length;
            offsets.micro = step->offsets.micro;
            if (!buffer_write(cursor, remaining, &offsets, sizeof(offsets))) {
                return false;
            }
        }

#if !SEQ_FEATURE_PLOCK_POOL
        for (uint8_t p = 0U; p < step->plock_count; ++p) {
            const seq_model_plock_t *plock = &step->plocks[p];
            track_plock_v1_payload_t payload;
            payload.value = plock->value;
            payload.parameter_id = plock->parameter_id;
            payload.domain = plock->domain;
            payload.voice_index = plock->voice_index;
            payload.internal_param = plock->internal_param;
            if (!buffer_write(cursor, remaining, &payload, sizeof(payload))) {
                return false;
            }
        }
#endif
#if SEQ_FEATURE_PLOCK_POOL
        if (!encode_plk2_chunk(step, cursor, remaining, write_plk2)) {
            return false;
        }
#endif

        ++step_count;
    }

    memcpy(count_ptr, &step_count, sizeof(step_count));
    return true;
}
#endif

#if BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
static bool encode_track_steps_v2(const seq_model_track_t *track,
                                  uint8_t **cursor,
                                  size_t *remaining,
                                  bool write_plk2) {
#if !SEQ_FEATURE_PLOCK_POOL
    (void)write_plk2;
#endif
    uint16_t step_count = 0U;
    uint8_t *count_ptr = *cursor;
    if (!buffer_write(cursor, remaining, &step_count, sizeof(step_count))) {
        return false;
    }

    int16_t previous_index = -1;

    for (uint8_t i = 0U; i < SEQ_MODEL_STEPS_PER_TRACK; ++i) {
        const seq_model_step_t *step = &track->steps[i];
        if (!step_needs_persist(step)) {
            continue;
        }

        const uint8_t skip = (uint8_t)(i - (uint8_t)(previous_index + 1));
        const uint8_t payload_mask = compute_voice_payload_mask(step);
        track_step_v2_header_t header;
        header.skip = skip;
        header.flags = 0U;
        header.voice_mask = 0U;
#if !SEQ_FEATURE_PLOCK_POOL
        header.plock_count = step->plock_count;
#else
        header.plock_count = 0U;
#endif

        if (step->flags.active) {
            header.flags |= STEP_FLAG_ACTIVE;
        }
        if (step->flags.automation) {
            header.flags |= STEP_FLAG_AUTOMATION;
        }
        if (!offsets_is_zero(&step->offsets)) {
            header.flags |= STEP_FLAG_OFFSETS;
        }
        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if (step->voices[v].state == SEQ_MODEL_VOICE_ENABLED) {
                header.voice_mask |= (uint8_t)(1U << v);
            }
        }

        header.flags |= (uint8_t)((payload_mask & 0x0FU) << 3);

        if (!buffer_write(cursor, remaining, &header, sizeof(header))) {
            return false;
        }

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if ((payload_mask & (uint8_t)(1U << v)) == 0U) {
                continue;
            }
            track_voice_v2_payload_t payload;
            const seq_model_voice_t *voice = &step->voices[v];
            payload.note = voice->note;
            payload.velocity = voice->velocity;
            payload.length = voice->length;
            payload.micro = voice->micro_offset;
            if (!buffer_write(cursor, remaining, &payload, sizeof(payload))) {
                return false;
            }
        }

        if ((header.flags & STEP_FLAG_OFFSETS) != 0U) {
            track_offsets_payload_t offsets;
            offsets.velocity = step->offsets.velocity;
            offsets.transpose = step->offsets.transpose;
            offsets.length = step->offsets.length;
            offsets.micro = step->offsets.micro;
            if (!buffer_write(cursor, remaining, &offsets, sizeof(offsets))) {
                return false;
            }
        }

#if !SEQ_FEATURE_PLOCK_POOL
        for (uint8_t p = 0U; p < step->plock_count; ++p) {
            const seq_model_plock_t *plock = &step->plocks[p];
            track_plock_v2_payload_t payload;
            uint8_t meta = (uint8_t)(plock->voice_index & 0x03U);
            if (plock->domain == SEQ_MODEL_PLOCK_CART) {
                meta |= (1U << 2);
            } else {
                meta |= (uint8_t)((plock->internal_param & 0x07U) << 3);
            }
            payload.value = plock->value;
            payload.meta = meta;
            if (!buffer_write(cursor, remaining, &payload, sizeof(payload))) {
                return false;
            }
            if (plock->domain == SEQ_MODEL_PLOCK_CART) {
                if (!buffer_write(cursor, remaining, &plock->parameter_id, sizeof(plock->parameter_id))) {
                    return false;
                }
            }
        }
#endif
#if SEQ_FEATURE_PLOCK_POOL
        if (!encode_plk2_chunk(step, cursor, remaining, write_plk2)) {
            return false;
        }
#endif

        previous_index = (int16_t)i;
        ++step_count;
    }

    memcpy(count_ptr, &step_count, sizeof(step_count));
    return true;
}
#endif

static bool encode_track_steps_dispatch(const seq_model_track_t *track,
                                        uint8_t **cursor,
                                        size_t *remaining,
                                        bool write_plk2) {
#if BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
    return encode_track_steps_v2(track, cursor, remaining, write_plk2);
#else
    return encode_track_steps_v1(track, cursor, remaining, write_plk2);
#endif
}

static bool track_steps_encode_internal(const seq_model_track_t *track,
                                        uint8_t *buffer,
                                        size_t buffer_size,
                                        size_t *written,
                                        bool write_plk2) {
    if ((buffer == NULL) || (written == NULL)) {
        return false;
    }

    uint8_t *cursor = buffer;
    size_t remaining = buffer_size;

    if (track == NULL) {
        if (buffer_size < sizeof(uint16_t)) {
            return false;
        }
        uint16_t zero = 0U;
        memcpy(cursor, &zero, sizeof(zero));
        *written = sizeof(zero);
        return true;
    }

    if (!encode_track_steps_dispatch(track, &cursor, &remaining, write_plk2)) {
        return false;
    }

    *written = buffer_size - remaining;
    return true;
}

static track_load_policy_t resolve_cart_policy(const seq_project_cart_ref_t *saved, seq_project_cart_ref_t *resolved) {
    *resolved = *saved;
    resolved->flags &= (uint8_t)~SEQ_PROJECT_CART_FLAG_MUTED;

    if (saved->cart_id == 0U) {
        return TRACK_LOAD_FULL;
    }

    const cart_id_t saved_slot = (cart_id_t)saved->slot_id;
    const uint32_t slot_uid = cart_registry_get_uid(saved_slot);
    if ((saved_slot < CART_COUNT) && (slot_uid == saved->cart_id)) {
        return TRACK_LOAD_FULL;
    }

    cart_id_t remapped;
    if (cart_registry_find_by_uid(saved->cart_id, &remapped)) {
        resolved->slot_id = (uint8_t)remapped;
        return TRACK_LOAD_REMAPPED;
    }

    if ((saved_slot < CART_COUNT) && cart_registry_is_present(saved_slot)) {
        resolved->flags |= SEQ_PROJECT_CART_FLAG_MUTED;
        return TRACK_LOAD_DIFFERENT_CART;
    }

    resolved->flags |= SEQ_PROJECT_CART_FLAG_MUTED;
    return TRACK_LOAD_ABSENT;
}

static bool decode_track_steps_v1(seq_model_track_t *track,
                                    const uint8_t *payload,
                                    uint32_t payload_size,
                                    track_load_policy_t policy) {
    const uint8_t *cursor = payload;
    size_t remaining = payload_size;

    uint16_t step_count = 0U;
    if (remaining < sizeof(step_count)) {
        return false;
    }
    memcpy(&step_count, cursor, sizeof(step_count));
    cursor += sizeof(step_count);
    remaining -= sizeof(step_count);

    seq_model_track_init(track);

    for (uint16_t s = 0U; s < step_count; ++s) {
        if (remaining < sizeof(track_step_v1_header_t)) {
            return false;
        }
        track_step_v1_header_t header;
        memcpy(&header, cursor, sizeof(header));
        cursor += sizeof(header);
        remaining -= sizeof(header);

        if (header.step_index >= SEQ_MODEL_STEPS_PER_TRACK) {
            return false;
        }

        seq_model_step_t *step = &track->steps[header.step_index];

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if (remaining < sizeof(track_voice_v1_payload_t)) {
                return false;
            }
            track_voice_v1_payload_t voice_payload;
            memcpy(&voice_payload, cursor, sizeof(voice_payload));
            cursor += sizeof(voice_payload);
            remaining -= sizeof(voice_payload);

            seq_model_voice_t *voice = &step->voices[v];
            voice->note = voice_payload.note;
            voice->velocity = voice_payload.velocity;
            voice->length = voice_payload.length;
            voice->micro_offset = voice_payload.micro;
            voice->state = voice_payload.state;
        }

        if ((header.flags & STEP_FLAG_OFFSETS) != 0U) {
            if (remaining < sizeof(track_offsets_payload_t)) {
                return false;
            }
            track_offsets_payload_t offsets;
            memcpy(&offsets, cursor, sizeof(offsets));
            cursor += sizeof(offsets);
            remaining -= sizeof(offsets);

            step->offsets.velocity = offsets.velocity;
            step->offsets.transpose = offsets.transpose;
            step->offsets.length = offsets.length;
            step->offsets.micro = offsets.micro;
        }

        uint8_t stored_plocks = header.plock_count;
        if (stored_plocks > SEQ_MODEL_MAX_PLOCKS_PER_STEP) {
            return false;
        }

        uint8_t effective_plocks = 0U;
        for (uint8_t p = 0U; p < stored_plocks; ++p) {
            if (remaining < sizeof(track_plock_v1_payload_t)) {
                return false;
            }
            track_plock_v1_payload_t payload_plock;
            memcpy(&payload_plock, cursor, sizeof(payload_plock));
            cursor += sizeof(payload_plock);
            remaining -= sizeof(payload_plock);

            if ((policy != TRACK_LOAD_FULL) && (policy != TRACK_LOAD_REMAPPED) &&
                (payload_plock.domain == SEQ_MODEL_PLOCK_CART)) {
                continue;
            }

#if !SEQ_FEATURE_PLOCK_POOL
            seq_model_plock_t *plock = &step->plocks[effective_plocks];
            plock->value = payload_plock.value;
            plock->parameter_id = payload_plock.parameter_id;
            plock->domain = payload_plock.domain;
            plock->voice_index = payload_plock.voice_index;
            plock->internal_param = payload_plock.internal_param;
            ++effective_plocks;
#else
            (void)policy;
#endif
        }
#if !SEQ_FEATURE_PLOCK_POOL
        step->plock_count = effective_plocks;
#else
        (void)effective_plocks;
#endif

        if (policy == TRACK_LOAD_ABSENT) {
            for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
                seq_model_voice_t *voice = &step->voices[v];
                voice->state = SEQ_MODEL_VOICE_DISABLED;
                voice->velocity = 0U;
            }
        }

        seq_model_step_recompute_flags(step);
    }

    return true;
}

static bool decode_track_steps_v2(seq_model_track_t *track,
                                    const uint8_t *payload,
                                    uint32_t payload_size,
                                    track_load_policy_t policy) {
    const uint8_t *cursor = payload;
    size_t remaining = payload_size;

    uint16_t step_count = 0U;
    if (remaining < sizeof(step_count)) {
        return false;
    }
    memcpy(&step_count, cursor, sizeof(step_count));
    cursor += sizeof(step_count);
    remaining -= sizeof(step_count);

    seq_model_track_init(track);
    int16_t current_index = -1;

    for (uint16_t s = 0U; s < step_count; ++s) {
        if (remaining < sizeof(track_step_v2_header_t)) {
            return false;
        }
        track_step_v2_header_t header;
        memcpy(&header, cursor, sizeof(header));
        cursor += sizeof(header);
        remaining -= sizeof(header);

        current_index += (int16_t)header.skip + 1;
        if ((current_index < 0) || (current_index >= (int16_t)SEQ_MODEL_STEPS_PER_TRACK)) {
            return false;
        }

        seq_model_step_t *step = &track->steps[current_index];
        seq_model_step_init(step);

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            seq_model_voice_t *voice = &step->voices[v];
            if ((header.voice_mask & (uint8_t)(1U << v)) != 0U) {
                voice->state = SEQ_MODEL_VOICE_ENABLED;
            }
        }

        const uint8_t payload_mask = (uint8_t)((header.flags >> 3) & 0x0FU);

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if ((payload_mask & (uint8_t)(1U << v)) == 0U) {
                continue;
            }
            if (remaining < sizeof(track_voice_v2_payload_t)) {
                return false;
            }
            track_voice_v2_payload_t voice_payload;
            memcpy(&voice_payload, cursor, sizeof(voice_payload));
            cursor += sizeof(voice_payload);
            remaining -= sizeof(voice_payload);

            seq_model_voice_t *voice = &step->voices[v];
            voice->note = voice_payload.note;
            voice->velocity = voice_payload.velocity;
            voice->length = voice_payload.length;
            voice->micro_offset = voice_payload.micro;
        }

        if ((header.flags & STEP_FLAG_OFFSETS) != 0U) {
            if (remaining < sizeof(track_offsets_payload_t)) {
                return false;
            }
            track_offsets_payload_t offsets;
            memcpy(&offsets, cursor, sizeof(offsets));
            cursor += sizeof(offsets);
            remaining -= sizeof(offsets);

            step->offsets.velocity = offsets.velocity;
            step->offsets.transpose = offsets.transpose;
            step->offsets.length = offsets.length;
            step->offsets.micro = offsets.micro;
        }

        uint8_t stored_plocks = header.plock_count;
        if (stored_plocks > SEQ_MODEL_MAX_PLOCKS_PER_STEP) {
            return false;
        }

        uint8_t effective_plocks = 0U;
        for (uint8_t p = 0U; p < stored_plocks; ++p) {
            if (remaining < sizeof(track_plock_v2_payload_t)) {
                return false;
            }
            track_plock_v2_payload_t payload_plock;
            memcpy(&payload_plock, cursor, sizeof(payload_plock));
            cursor += sizeof(payload_plock);
            remaining -= sizeof(payload_plock);

            const bool is_cart = ((payload_plock.meta & (1U << 2)) != 0U);
            uint16_t parameter_id = 0U;
            if (is_cart) {
                if (remaining < sizeof(parameter_id)) {
                    return false;
                }
                memcpy(&parameter_id, cursor, sizeof(parameter_id));
                cursor += sizeof(parameter_id);
                remaining -= sizeof(parameter_id);
            }

            if ((policy != TRACK_LOAD_FULL) && (policy != TRACK_LOAD_REMAPPED) && is_cart) {
                continue;
            }

#if !SEQ_FEATURE_PLOCK_POOL
            if (effective_plocks >= SEQ_MODEL_MAX_PLOCKS_PER_STEP) {
                return false;
            }

            seq_model_plock_t *plock = &step->plocks[effective_plocks];
            plock->value = payload_plock.value;
            plock->voice_index = (uint8_t)(payload_plock.meta & 0x03U);
            if (is_cart) {
                plock->domain = SEQ_MODEL_PLOCK_CART;
                plock->parameter_id = parameter_id;
                plock->internal_param = 0U;
            } else {
                plock->domain = SEQ_MODEL_PLOCK_INTERNAL;
                plock->parameter_id = 0U;
                plock->internal_param = (uint8_t)((payload_plock.meta >> 3) & 0x07U);
            }
            ++effective_plocks;
#else
            (void)parameter_id;
#endif
        }
#if !SEQ_FEATURE_PLOCK_POOL
        step->plock_count = effective_plocks;
#else
        (void)effective_plocks;
#endif

        if (policy == TRACK_LOAD_ABSENT) {
            for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
                seq_model_voice_t *voice = &step->voices[v];
                voice->state = SEQ_MODEL_VOICE_DISABLED;
                voice->velocity = 0U;
            }
        }

        if ((header.flags & STEP_FLAG_ACTIVE) != 0U) {
            step->flags.active = true;
        }
        if ((header.flags & STEP_FLAG_AUTOMATION) != 0U) {
            step->flags.automation = true;
        }

        seq_model_step_recompute_flags(step);
    }

    return true;
}

bool seq_project_track_steps_encode(const seq_model_track_t *track,
                                      uint8_t *buffer,
                                      size_t buffer_size,
                                      size_t *written) {
    bool write_plk2 = false;
#if SEQ_FEATURE_PLOCK_POOL
    write_plk2 = true;
#endif
    return track_steps_encode_internal(track, buffer, buffer_size, written, write_plk2);
}

ssize_t seq_codec_write_track_with_plk2(uint8_t *dst,
                                        size_t cap,
                                        const seq_model_track_t *track,
                                        int enable_plk2) {
    size_t written = 0U;
    bool write_plk2 = false;
#if SEQ_FEATURE_PLOCK_POOL
    write_plk2 = (enable_plk2 != 0);
#else
    (void)enable_plk2;
#endif
    if (!track_steps_encode_internal(track, dst, cap, &written, write_plk2)) {
        return -1;
    }
    return (ssize_t)written;
}

bool seq_project_track_steps_decode(seq_model_track_t *track,
                                      const uint8_t *buffer,
                                      size_t buffer_size,
                                      uint8_t version,
                                      seq_project_track_decode_policy_t policy_mode) {
    if ((track == NULL) || (buffer == NULL)) {
        return false;
    }

    track_load_policy_t policy = TRACK_LOAD_FULL;
    switch (policy_mode) {
    case SEQ_PROJECT_TRACK_DECODE_FULL:
        policy = TRACK_LOAD_FULL;
        break;
    case SEQ_PROJECT_TRACK_DECODE_DROP_CART:
        policy = TRACK_LOAD_DIFFERENT_CART;
        break;
    case SEQ_PROJECT_TRACK_DECODE_ABSENT:
        policy = TRACK_LOAD_ABSENT;
        break;
    default:
        return false;
    }

    switch (version) {
    case 1U:
        return decode_track_steps_v1(track, buffer, (uint32_t)buffer_size, policy);
    case 2U:
        return decode_track_steps_v2(track, buffer, (uint32_t)buffer_size, policy);
    default:
        break;
    }

    return false;
}

void seq_project_init(seq_project_t *project) {
    if (project == NULL) {
        return;
    }

    memset(project, 0, sizeof(*project));
    for (uint8_t b = 0U; b < SEQ_PROJECT_BANK_COUNT; ++b) {
        for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
            pattern_desc_reset(&project->banks[b].patterns[p]);
        }
    }

    project->tempo = 120U;
    project->project_index = 0U;
    seq_model_gen_reset(&project->generation);
    project_bind(project);
    (void)ensure_flash_ready();
}

bool seq_project_assign_track(seq_project_t *project, uint8_t track_index, seq_model_track_t *track) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return false;
    }

    project->tracks[track_index].track = track;
    if ((track != NULL) && (track_index + 1U > project->track_count)) {
        project->track_count = (uint8_t)(track_index + 1U);
    }

    if ((project->active_track >= project->track_count) ||
        (project->tracks[project->active_track].track == NULL)) {
        project->active_track = track_index;
    }

    seq_project_bump_generation(project);
    return true;
}

seq_model_track_t *seq_project_get_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].track;
}

const seq_model_track_t *seq_project_get_track_const(const seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].track;
}

bool seq_project_set_active_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return false;
    }
    if (project->tracks[track_index].track == NULL) {
        return false;
    }
    if (project->active_track == track_index) {
        return true;
    }

    project->active_track = track_index;
    seq_project_bump_generation(project);
    return true;
}

uint8_t seq_project_get_active_track_index(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    if (project->active_track >= project->track_count) {
        return 0U;
    }
    return project->active_track;
}

seq_model_track_t *seq_project_get_active_track(seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track(project, seq_project_get_active_track_index(project)) : NULL;
}

const seq_model_track_t *seq_project_get_active_track_const(const seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track_const(project, seq_project_get_active_track_index(project)) : NULL;
}

uint8_t seq_project_get_track_count(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    return project->track_count;
}

void seq_project_clear_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return;
    }

    project->tracks[track_index].track = NULL;
    memset(&project->tracks[track_index].cart, 0, sizeof(project->tracks[track_index].cart));

    while ((project->track_count > 0U) &&
           (project->tracks[project->track_count - 1U].track == NULL)) {
        project->track_count--;
    }

    if (project->active_track >= project->track_count) {
        project->active_track = 0U;
    }

    seq_project_bump_generation(project);
}

void seq_project_bump_generation(seq_project_t *project) {
    if (project == NULL) {
        return;
    }
    seq_model_gen_bump(&project->generation);
}

const seq_model_gen_t *seq_project_get_generation(const seq_project_t *project) {
    if (project == NULL) {
        return NULL;
    }
    return &project->generation;
}

void seq_project_set_track_cart(seq_project_t *project, uint8_t track_index, const seq_project_cart_ref_t *cart) {
    if ((project == NULL) || (cart == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return;
    }
    project->tracks[track_index].cart = *cart;
}

const seq_project_cart_ref_t *seq_project_get_track_cart(const seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return NULL;
    }
    return &project->tracks[track_index].cart;
}

bool seq_project_set_active_slot(seq_project_t *project, uint8_t bank, uint8_t pattern) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return false;
    }
    if ((project->active_bank == bank) && (project->active_pattern == pattern)) {
        return true;
    }

    project->active_bank = bank;
    project->active_pattern = pattern;
    seq_project_bump_generation(project);
    return true;
}

uint8_t seq_project_get_active_bank(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    return project->active_bank;
}

uint8_t seq_project_get_active_pattern_index(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    return project->active_pattern;
}

seq_project_pattern_desc_t *seq_project_get_pattern_descriptor(seq_project_t *project, uint8_t bank, uint8_t pattern) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return NULL;
    }
    return &project->banks[bank].patterns[pattern];
}

const seq_project_pattern_desc_t *seq_project_get_pattern_descriptor_const(const seq_project_t *project, uint8_t bank, uint8_t pattern) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return NULL;
    }
    return &project->banks[bank].patterns[pattern];
}

static bool update_directory(const seq_project_t *project, uint8_t project_index) {
    seq_project_directory_t dir;
    memset(&dir, 0, sizeof(dir));

    dir.magic = SEQ_PROJECT_DIRECTORY_MAGIC;
    dir.version = SEQ_PROJECT_DIRECTORY_VERSION;
    dir.project_index = project_index;
    dir.tempo = project->tempo;
    dir.active_bank = project->active_bank;
    dir.active_pattern = project->active_pattern;
    dir.track_count = project->track_count;
    memcpy(dir.name, project->name, sizeof(dir.name));

    const uint32_t base = project_base(project_index);

    for (uint8_t b = 0U; b < SEQ_PROJECT_BANK_COUNT; ++b) {
        for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
            const seq_project_pattern_desc_t *desc = &project->banks[b].patterns[p];
            seq_project_directory_entry_t *entry = &dir.entries[b][p];
            entry->version = desc->version;
            entry->track_count = desc->track_count;
            if ((desc->storage_length > 0U) && (desc->storage_offset >= base)) {
                entry->offset = desc->storage_offset - base;
                entry->length = desc->storage_length;
            } else {
                entry->offset = 0U;
                entry->length = 0U;
            }
        }
    }

    if (!board_flash_erase(base, sizeof(dir))) {
        return false;
    }
    if (!board_flash_write(base, &dir, sizeof(dir))) {
        return false;
    }
    return true;
}

bool seq_project_save(uint8_t project_index) {
    if ((s_active_project == NULL) || (project_index >= SEQ_PROJECT_MAX_PROJECTS)) {
        return false;
    }
    if (!ensure_flash_ready()) {
        return false;
    }

    const seq_project_t *project_ro = s_active_project;
    seq_cold_view_t project_view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    if ((project_view._p != NULL) && (project_view._bytes >= sizeof(seq_project_t))) {
        project_ro = (const seq_project_t *)project_view._p;
    }

    if (!update_directory(project_ro, project_index)) {
        return false;
    }

    s_active_project->project_index = project_index;
    return true;
}

bool seq_project_load(uint8_t project_index) {
    if ((s_active_project == NULL) || (project_index >= SEQ_PROJECT_MAX_PROJECTS)) {
        return false;
    }
    if (!ensure_flash_ready()) {
        return false;
    }

    seq_project_directory_t dir;
    const uint32_t base = project_base(project_index);
    if (!board_flash_read(base, &dir, sizeof(dir))) {
        return false;
    }

    if ((dir.magic != SEQ_PROJECT_DIRECTORY_MAGIC) || (dir.version != SEQ_PROJECT_DIRECTORY_VERSION)) {
        return false;
    }

    seq_project_t *project = s_active_project;
    project->project_index = project_index;
    project->tempo = dir.tempo;
    project->active_bank = (dir.active_bank < SEQ_PROJECT_BANK_COUNT) ? dir.active_bank : 0U;
    project->active_pattern = (dir.active_pattern < SEQ_PROJECT_PATTERNS_PER_BANK) ? dir.active_pattern : 0U;
    project->track_count = (dir.track_count <= SEQ_PROJECT_MAX_TRACKS) ? dir.track_count : SEQ_PROJECT_MAX_TRACKS;
    memcpy(project->name, dir.name, sizeof(project->name));

    for (uint8_t b = 0U; b < SEQ_PROJECT_BANK_COUNT; ++b) {
        for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
            seq_project_pattern_desc_t *desc = &project->banks[b].patterns[p];
            pattern_desc_reset(desc);
            const seq_project_directory_entry_t *entry = &dir.entries[b][p];
            desc->version = entry->version;
            desc->track_count = (entry->track_count <= SEQ_PROJECT_MAX_TRACKS) ? entry->track_count : SEQ_PROJECT_MAX_TRACKS;
            if (entry->length > 0U) {
                desc->storage_offset = base + entry->offset;
                desc->storage_length = entry->length;
            }
        }
    }

    seq_project_bump_generation(project);
    return true;
}

bool seq_pattern_save(uint8_t bank, uint8_t pattern) {
    if ((s_active_project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return false;
    }
    if (!ensure_flash_ready()) {
        return false;
    }

    seq_project_t *project = s_active_project;
    seq_project_pattern_desc_t *desc = &project->banks[bank].patterns[pattern];

    const seq_project_t *project_ro = project;
    seq_cold_view_t project_view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    if ((project_view._p != NULL) && (project_view._bytes >= sizeof(seq_project_t))) {
        project_ro = (const seq_project_t *)project_view._p;
    }

    seq_cold_view_t cart_meta_view = seq_runtime_cold_view(SEQ_COLDV_CART_META);
    const seq_project_track_t *tracks_meta = project_ro->tracks;
    size_t tracks_meta_capacity = project_ro->track_count;

    if ((cart_meta_view._p != NULL) && (cart_meta_view._bytes >= sizeof(seq_project_track_t))) {
        const size_t view_count = cart_meta_view._bytes / sizeof(seq_project_track_t);
        if (view_count >= (size_t)project_ro->track_count) {
            tracks_meta = (const seq_project_track_t *)cart_meta_view._p;
            tracks_meta_capacity = view_count;
        }
    }

    const size_t meta_limit = (tracks_meta_capacity < (size_t)project_ro->track_count)
                                  ? tracks_meta_capacity
                                  : (size_t)project_ro->track_count;
    uint8_t track_count = 0U;
    for (uint8_t i = 0U; i < (uint8_t)meta_limit; ++i) {
        if (tracks_meta[i].track != NULL) {
            track_count = (uint8_t)(i + 1U);
        }
    }

    uint8_t *cursor = s_pattern_buffer;
    size_t remaining = sizeof(s_pattern_buffer);

    pattern_blob_header_t header = {
        .magic = SEQ_PROJECT_PATTERN_MAGIC,
        .version = SEQ_PROJECT_PATTERN_VERSION,
        .track_count = track_count,
        .reserved = 0U
    };

    if (!buffer_write(&cursor, &remaining, &header, sizeof(header))) {
        return false;
    }

    for (uint8_t track = 0U; track < track_count; ++track) {
        const seq_project_track_t *track_meta = &tracks_meta[track];
        const seq_model_track_t *track_ptr = track_meta->track;
        track_payload_header_t track_header = {
            .cart_id = track_meta->cart.cart_id,
            .payload_size = 0U,
            .slot_id = track_meta->cart.slot_id,
            .flags = track_meta->cart.flags,
            .capabilities = track_meta->cart.capabilities
        };

        const size_t header_pos = (size_t)(cursor - s_pattern_buffer);
        if (!buffer_write(&cursor, &remaining, &track_header, sizeof(track_header))) {
            return false;
        }
        const size_t payload_start = (size_t)(cursor - s_pattern_buffer);

        if (track_ptr != NULL) {
            size_t written = 0U;
            if (!seq_project_track_steps_encode(track_ptr, cursor, remaining, &written)) {
                return false;
            }
            cursor += written;
            remaining -= written;
        }

        const size_t payload_size = (size_t)(cursor - s_pattern_buffer) - payload_start;
        track_payload_header_t stored_header = track_header;
        stored_header.payload_size = (uint32_t)payload_size;
        memcpy(&s_pattern_buffer[header_pos], &stored_header, sizeof(stored_header));
    }

    const size_t total_size = (size_t)(cursor - s_pattern_buffer);
    if (total_size > SEQ_PROJECT_PATTERN_STORAGE_MAX) {
        return false;
    }

    const uint32_t offset = pattern_offset(project_ro->project_index, bank, pattern);
    if (!board_flash_erase(offset, SEQ_PROJECT_PATTERN_STORAGE_MAX)) {
        return false;
    }
    if (!board_flash_write(offset, s_pattern_buffer, total_size)) {
        return false;
    }

    desc->version = SEQ_PROJECT_PATTERN_VERSION;
    desc->track_count = track_count;
    desc->storage_offset = offset;
    desc->storage_length = (uint32_t)total_size;

    for (uint8_t t = 0U; t < SEQ_PROJECT_MAX_TRACKS; ++t) {
        if (t < track_count) {
            desc->tracks[t].cart = project_ro->tracks[t].cart;
            desc->tracks[t].valid = 1U;
        } else {
            memset(&desc->tracks[t].cart, 0, sizeof(desc->tracks[t].cart));
            desc->tracks[t].valid = 0U;
        }
    }

    seq_project_bump_generation(project);
    return seq_project_save(project_ro->project_index);
}

bool seq_pattern_load(uint8_t bank, uint8_t pattern) {
    if ((s_active_project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return false;
    }
    if (!ensure_flash_ready()) {
        return false;
    }

    seq_project_t *project = s_active_project;
    seq_project_pattern_desc_t *desc = &project->banks[bank].patterns[pattern];

    const seq_project_t *project_ro = project;
    seq_cold_view_t project_view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    if ((project_view._p != NULL) && (project_view._bytes >= sizeof(seq_project_t))) {
        project_ro = (const seq_project_t *)project_view._p;
    }

    if ((desc->storage_length == 0U) || (desc->storage_offset == 0U)) {
        for (uint8_t t = 0U; t < project_ro->track_count; ++t) {
            seq_model_track_t *track_model = project_ro->tracks[t].track;
            if (track_model != NULL) {
                seq_model_track_init(track_model);
            }
        }
        return true;
    }

    if (desc->storage_length > SEQ_PROJECT_PATTERN_STORAGE_MAX) {
        return false;
    }

    if (!board_flash_read(desc->storage_offset, s_pattern_buffer, desc->storage_length)) {
        return false;
    }

    const uint8_t *cursor = s_pattern_buffer;
    size_t remaining = desc->storage_length;

    if (remaining < sizeof(pattern_blob_header_t)) {
        return false;
    }
    pattern_blob_header_t header;
    memcpy(&header, cursor, sizeof(header));
    cursor += sizeof(header);
    remaining -= sizeof(header);

    if (header.magic != SEQ_PROJECT_PATTERN_MAGIC) {
        return false;
    }
    if ((header.version != 1U) && (header.version != 2U)) {
        return false;
    }

    const uint8_t stored_tracks = (header.track_count <= SEQ_PROJECT_MAX_TRACKS) ? header.track_count : SEQ_PROJECT_MAX_TRACKS;

    for (uint8_t track = 0U; track < stored_tracks; ++track) {
        if (remaining < sizeof(track_payload_header_t)) {
            return false;
        }
        track_payload_header_t track_header;
        memcpy(&track_header, cursor, sizeof(track_header));
        cursor += sizeof(track_header);
        remaining -= sizeof(track_header);

        if (track_header.payload_size > remaining) {
            return false;
        }

        seq_project_cart_ref_t saved_cart;
        memset(&saved_cart, 0, sizeof(saved_cart));
        saved_cart.cart_id = track_header.cart_id;
        saved_cart.slot_id = track_header.slot_id;
        saved_cart.capabilities = track_header.capabilities;
        saved_cart.flags = track_header.flags;

        seq_project_cart_ref_t resolved_cart;
        track_load_policy_t policy = resolve_cart_policy(&saved_cart, &resolved_cart);
        seq_project_track_decode_policy_t decode_policy = SEQ_PROJECT_TRACK_DECODE_FULL;
        switch (policy) {
        case TRACK_LOAD_FULL:
        case TRACK_LOAD_REMAPPED:
            decode_policy = SEQ_PROJECT_TRACK_DECODE_FULL;
            break;
        case TRACK_LOAD_DIFFERENT_CART:
            decode_policy = SEQ_PROJECT_TRACK_DECODE_DROP_CART;
            break;
        case TRACK_LOAD_ABSENT:
            decode_policy = SEQ_PROJECT_TRACK_DECODE_ABSENT;
            break;
        }

        if (track < project_ro->track_count) {
            seq_model_track_t *track_model = project_ro->tracks[track].track;
            if ((track_model != NULL) &&
                !seq_project_track_steps_decode(track_model, cursor, track_header.payload_size, header.version, decode_policy)) {
                return false;
            }
            project->tracks[track].cart = resolved_cart;
        }

        if (track < SEQ_PROJECT_MAX_TRACKS) {
            desc->tracks[track].cart = resolved_cart;
            desc->tracks[track].valid = 1U;
        }

        cursor += track_header.payload_size;
        remaining -= track_header.payload_size;
    }

    if (stored_tracks > project_ro->track_count) {
        project->track_count = stored_tracks;
    }
    desc->track_count = stored_tracks;
    for (uint8_t t = stored_tracks; t < SEQ_PROJECT_MAX_TRACKS; ++t) {
        desc->tracks[t].valid = 0U;
        memset(&desc->tracks[t].cart, 0, sizeof(desc->tracks[t].cart));
    }
    seq_project_bump_generation(project);
    return true;
}
