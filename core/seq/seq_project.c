/**
 * @file seq_project.c
 * @brief Sequencer multi-track project helpers implementation.
 */

#include "seq_project.h"

#include <string.h>

#include "brick_config.h"
#include "board/board_flash.h"
#include "cart/cart_registry.h"

#define SEQ_PROJECT_DIRECTORY_MAGIC 0x4250524FU /* 'BPRO' */
#define SEQ_PROJECT_PATTERN_MAGIC   0x42504154U /* 'BPAT' */
#define SEQ_PROJECT_DIRECTORY_VERSION 1U
#define SEQ_PROJECT_PATTERN_VERSION   1U

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
} pattern_track_header_t;

typedef struct __attribute__((packed)) {
    uint8_t step_index;  /**< Step index inside the pattern. */
    uint8_t flags;       /**< Step flags bitmask. */
    uint8_t voice_mask;  /**< Mask of enabled voices. */
    uint8_t plock_count; /**< Number of serialized parameter locks. */
} pattern_step_header_t;

typedef struct __attribute__((packed)) {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    int8_t  micro;
    uint8_t state;
} pattern_voice_payload_t;

typedef struct __attribute__((packed)) {
    int16_t velocity;
    int8_t  transpose;
    int8_t  length;
    int8_t  micro;
} pattern_offsets_payload_t;

typedef struct __attribute__((packed)) {
    int16_t value;
    uint16_t parameter_id;
    uint8_t domain;
    uint8_t voice_index;
    uint8_t internal_param;
} pattern_plock_payload_t;

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

static void project_cache_reset(seq_project_t *project) {
    if (project == NULL) {
        return;
    }
    for (uint8_t i = 0U; i < SEQ_PROJECT_PATTERNS_PER_BANK; ++i) {
        pattern_desc_reset(&project->bank_cache[i]);
    }
    project->bank_cache_index = 0U;
    project->bank_cache_valid = 0U;
}

static void project_cache_populate(seq_project_t *project,
                                   uint8_t bank,
                                   const seq_project_directory_t *dir,
                                   uint32_t base) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT)) {
        return;
    }

    for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
        pattern_desc_reset(&project->bank_cache[p]);
    }

    if ((dir != NULL) &&
        (dir->magic == SEQ_PROJECT_DIRECTORY_MAGIC) &&
        (dir->version == SEQ_PROJECT_DIRECTORY_VERSION)) {
        for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
            seq_project_pattern_desc_t *desc = &project->bank_cache[p];
            const seq_project_directory_entry_t *entry = &dir->entries[bank][p];
            desc->version = entry->version;
            desc->track_count = (entry->track_count <= SEQ_PROJECT_MAX_TRACKS)
                                     ? entry->track_count
                                     : SEQ_PROJECT_MAX_TRACKS;
            if ((entry->length > 0U) &&
                (entry->offset < SEQ_PROJECT_FLASH_SLOT_SIZE) &&
                (entry->offset + entry->length <= SEQ_PROJECT_FLASH_SLOT_SIZE)) {
                desc->storage_length = entry->length;
                desc->storage_offset = base + entry->offset;
            }
        }
    }

    project->bank_cache_index = bank;
    project->bank_cache_valid = 1U;
}

static bool project_cache_load(seq_project_t *project, uint8_t bank) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT)) {
        return false;
    }
    if (project->bank_cache_valid && (project->bank_cache_index == bank)) {
        return true;
    }

    project->bank_cache_valid = 0U;

    if (project->project_index >= SEQ_PROJECT_MAX_PROJECTS) {
        project_cache_populate(project, bank, NULL, 0U);
        return true;
    }

    if (!ensure_flash_ready()) {
        project_cache_populate(project, bank, NULL, 0U);
        return false;
    }

    const uint32_t base = project_base(project->project_index);
    seq_project_directory_t dir;
    if (!board_flash_read(base, &dir, sizeof(dir))) {
        project_cache_populate(project, bank, NULL, base);
        return false;
    }

    project_cache_populate(project, bank, &dir, base);
    return true;
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
    if (step->plock_count > 0U) {
        return true;
    }
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

static bool buffer_write(uint8_t **cursor, size_t *remaining, const void *src, size_t len) {
    if (*remaining < len) {
        return false;
    }
    memcpy(*cursor, src, len);
    *cursor += len;
    *remaining -= len;
    return true;
}

static bool encode_pattern_steps(const seq_model_pattern_t *pattern, uint8_t **cursor, size_t *remaining) {
    uint16_t step_count = 0U;
    uint8_t *count_ptr = *cursor;
    if (!buffer_write(cursor, remaining, &step_count, sizeof(step_count))) {
        return false;
    }

    for (uint8_t i = 0U; i < SEQ_MODEL_STEPS_PER_PATTERN; ++i) {
        const seq_model_step_t *step = &pattern->steps[i];
        if (!step_needs_persist(step)) {
            continue;
        }

        pattern_step_header_t header;
        header.step_index = i;
        header.flags = 0U;
        header.voice_mask = 0U;
        header.plock_count = step->plock_count;

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
            pattern_voice_payload_t payload;
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
            pattern_offsets_payload_t offsets;
            offsets.velocity = step->offsets.velocity;
            offsets.transpose = step->offsets.transpose;
            offsets.length = step->offsets.length;
            offsets.micro = step->offsets.micro;
            if (!buffer_write(cursor, remaining, &offsets, sizeof(offsets))) {
                return false;
            }
        }

        for (uint8_t p = 0U; p < step->plock_count; ++p) {
            const seq_model_plock_t *plock = &step->plocks[p];
            pattern_plock_payload_t payload;
            payload.value = plock->value;
            payload.parameter_id = plock->parameter_id;
            payload.domain = plock->domain;
            payload.voice_index = plock->voice_index;
            payload.internal_param = plock->internal_param;
            if (!buffer_write(cursor, remaining, &payload, sizeof(payload))) {
                return false;
            }
        }

        ++step_count;
    }

    memcpy(count_ptr, &step_count, sizeof(step_count));
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

static bool decode_pattern_steps(seq_model_pattern_t *pattern,
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

    seq_model_pattern_init(pattern);

    for (uint16_t s = 0U; s < step_count; ++s) {
        if (remaining < sizeof(pattern_step_header_t)) {
            return false;
        }
        pattern_step_header_t header;
        memcpy(&header, cursor, sizeof(header));
        cursor += sizeof(header);
        remaining -= sizeof(header);

        if (header.step_index >= SEQ_MODEL_STEPS_PER_PATTERN) {
            return false;
        }

        seq_model_step_t *step = &pattern->steps[header.step_index];

        for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
            if (remaining < sizeof(pattern_voice_payload_t)) {
                return false;
            }
            pattern_voice_payload_t voice_payload;
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
            if (remaining < sizeof(pattern_offsets_payload_t)) {
                return false;
            }
            pattern_offsets_payload_t offsets;
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
            if (remaining < sizeof(pattern_plock_payload_t)) {
                return false;
            }
            pattern_plock_payload_t payload_plock;
            memcpy(&payload_plock, cursor, sizeof(payload_plock));
            cursor += sizeof(payload_plock);
            remaining -= sizeof(payload_plock);

            if ((policy != TRACK_LOAD_FULL) && (policy != TRACK_LOAD_REMAPPED) &&
                (payload_plock.domain == SEQ_MODEL_PLOCK_CART)) {
                continue;
            }

            seq_model_plock_t *plock = &step->plocks[effective_plocks];
            plock->value = payload_plock.value;
            plock->parameter_id = payload_plock.parameter_id;
            plock->domain = payload_plock.domain;
            plock->voice_index = payload_plock.voice_index;
            plock->internal_param = payload_plock.internal_param;
            ++effective_plocks;
        }
        step->plock_count = effective_plocks;

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

void seq_project_init(seq_project_t *project) {
    if (project == NULL) {
        return;
    }

    memset(project, 0, sizeof(*project));
    project_cache_reset(project);

    project->tempo = 120U;
    project->project_index = 0U;
    seq_model_gen_reset(&project->generation);
    project_bind(project);
    (void)ensure_flash_ready();
}

bool seq_project_assign_track(seq_project_t *project, uint8_t track_index, seq_model_pattern_t *pattern) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return false;
    }

    project->tracks[track_index].pattern = pattern;
    if ((pattern != NULL) && (track_index + 1U > project->track_count)) {
        project->track_count = (uint8_t)(track_index + 1U);
    }

    if ((project->active_track >= project->track_count) ||
        (project->tracks[project->active_track].pattern == NULL)) {
        project->active_track = track_index;
    }

    seq_project_bump_generation(project);
    return true;
}

seq_model_pattern_t *seq_project_get_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].pattern;
}

const seq_model_pattern_t *seq_project_get_track_const(const seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].pattern;
}

bool seq_project_set_active_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return false;
    }
    if (project->tracks[track_index].pattern == NULL) {
        return false;
    }
    if (project->active_track == track_index) {
        return true;
    }

    project->active_track = track_index;
    seq_project_bump_generation(project);
    return true;
}

uint8_t seq_project_get_active_track(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    if (project->active_track >= project->track_count) {
        return 0U;
    }
    return project->active_track;
}

seq_model_pattern_t *seq_project_get_active_pattern(seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track(project, seq_project_get_active_track(project)) : NULL;
}

const seq_model_pattern_t *seq_project_get_active_pattern_const(const seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track_const(project, seq_project_get_active_track(project)) : NULL;
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

    project->tracks[track_index].pattern = NULL;
    memset(&project->tracks[track_index].cart, 0, sizeof(project->tracks[track_index].cart));

    while ((project->track_count > 0U) &&
           (project->tracks[project->track_count - 1U].pattern == NULL)) {
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

    (void)project_cache_load(project, bank);

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
    if (!project_cache_load(project, bank)) {
        return NULL;
    }
    return &project->bank_cache[pattern];
}

const seq_project_pattern_desc_t *seq_project_get_pattern_descriptor_const(const seq_project_t *project, uint8_t bank, uint8_t pattern) {
    if ((project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return NULL;
    }
    if (!project_cache_load((seq_project_t *)project, bank)) {
        return NULL;
    }
    return &project->bank_cache[pattern];
}

static bool update_directory(const seq_project_t *project, uint8_t project_index) {
    const uint32_t base = project_base(project_index);
    seq_project_directory_t dir;
    bool has_valid = false;

    if (board_flash_read(base, &dir, sizeof(dir))) {
        has_valid = (dir.magic == SEQ_PROJECT_DIRECTORY_MAGIC) &&
                    (dir.version == SEQ_PROJECT_DIRECTORY_VERSION);
    }

    if (!has_valid) {
        memset(&dir, 0, sizeof(dir));
    }

    dir.magic = SEQ_PROJECT_DIRECTORY_MAGIC;
    dir.version = SEQ_PROJECT_DIRECTORY_VERSION;
    dir.project_index = project_index;
    dir.tempo = project->tempo;
    dir.active_bank = project->active_bank;
    dir.active_pattern = project->active_pattern;
    dir.track_count = project->track_count;
    memcpy(dir.name, project->name, sizeof(dir.name));

    if (project->bank_cache_valid && (project->bank_cache_index < SEQ_PROJECT_BANK_COUNT)) {
        const uint8_t bank = project->bank_cache_index;
        for (uint8_t p = 0U; p < SEQ_PROJECT_PATTERNS_PER_BANK; ++p) {
            const seq_project_pattern_desc_t *desc = &project->bank_cache[p];
            seq_project_directory_entry_t *entry = &dir.entries[bank][p];
            entry->version = desc->version;
            entry->track_count = desc->track_count;
            if ((desc->storage_length > 0U) &&
                (desc->storage_offset >= base) &&
                (desc->storage_offset - base < SEQ_PROJECT_FLASH_SLOT_SIZE)) {
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

    (void)project_cache_load(s_active_project, s_active_project->active_bank);

    if (!update_directory(s_active_project, project_index)) {
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

    project_cache_populate(project, project->active_bank, &dir, base);

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
    seq_project_pattern_desc_t *desc = seq_project_get_pattern_descriptor(project, bank, pattern);
    if (desc == NULL) {
        return false;
    }

    uint8_t track_count = 0U;
    for (uint8_t i = 0U; i < project->track_count; ++i) {
        if (project->tracks[i].pattern != NULL) {
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
        const seq_model_pattern_t *pattern_ptr = project->tracks[track].pattern;
        pattern_track_header_t track_header = {
            .cart_id = project->tracks[track].cart.cart_id,
            .payload_size = 0U,
            .slot_id = project->tracks[track].cart.slot_id,
            .flags = project->tracks[track].cart.flags,
            .capabilities = project->tracks[track].cart.capabilities
        };

        const size_t header_pos = (size_t)(cursor - s_pattern_buffer);
        if (!buffer_write(&cursor, &remaining, &track_header, sizeof(track_header))) {
            return false;
        }
        const size_t payload_start = (size_t)(cursor - s_pattern_buffer);

        if ((pattern_ptr != NULL) && !encode_pattern_steps(pattern_ptr, &cursor, &remaining)) {
            return false;
        }

        const size_t payload_size = (size_t)(cursor - s_pattern_buffer) - payload_start;
        pattern_track_header_t stored_header = track_header;
        stored_header.payload_size = (uint32_t)payload_size;
        memcpy(&s_pattern_buffer[header_pos], &stored_header, sizeof(stored_header));
    }

    const size_t total_size = (size_t)(cursor - s_pattern_buffer);
    if (total_size > SEQ_PROJECT_PATTERN_STORAGE_MAX) {
        return false;
    }

    const uint32_t offset = pattern_offset(project->project_index, bank, pattern);
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
            desc->tracks[t].cart = project->tracks[t].cart;
            desc->tracks[t].valid = 1U;
        } else {
            memset(&desc->tracks[t].cart, 0, sizeof(desc->tracks[t].cart));
            desc->tracks[t].valid = 0U;
        }
    }

    seq_project_bump_generation(project);
    return seq_project_save(project->project_index);
}

bool seq_pattern_load(uint8_t bank, uint8_t pattern) {
    if ((s_active_project == NULL) || (bank >= SEQ_PROJECT_BANK_COUNT) || (pattern >= SEQ_PROJECT_PATTERNS_PER_BANK)) {
        return false;
    }
    if (!ensure_flash_ready()) {
        return false;
    }

    seq_project_t *project = s_active_project;
    seq_project_pattern_desc_t *desc = seq_project_get_pattern_descriptor(project, bank, pattern);
    if (desc == NULL) {
        return false;
    }

    if ((desc->storage_length == 0U) || (desc->storage_offset == 0U)) {
        for (uint8_t t = 0U; t < project->track_count; ++t) {
            seq_model_pattern_t *pat = project->tracks[t].pattern;
            if (pat != NULL) {
                seq_model_pattern_init(pat);
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

    if ((header.magic != SEQ_PROJECT_PATTERN_MAGIC) || (header.version != SEQ_PROJECT_PATTERN_VERSION)) {
        return false;
    }

    const uint8_t stored_tracks = (header.track_count <= SEQ_PROJECT_MAX_TRACKS) ? header.track_count : SEQ_PROJECT_MAX_TRACKS;

    for (uint8_t track = 0U; track < stored_tracks; ++track) {
        if (remaining < sizeof(pattern_track_header_t)) {
            return false;
        }
        pattern_track_header_t track_header;
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

        if (track < project->track_count) {
            seq_model_pattern_t *pat = project->tracks[track].pattern;
            if ((pat != NULL) && !decode_pattern_steps(pat, cursor, track_header.payload_size, policy)) {
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

    if (stored_tracks > project->track_count) {
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
