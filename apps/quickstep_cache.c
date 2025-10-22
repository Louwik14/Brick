#include "apps/quickstep_cache.h"

#include <string.h>

#include "apps/rtos_shim.h"
#include "brick_config.h"
#include "core/seq/seq_model.h"

enum {
    k_quickstep_track_count = 16U,
    k_quickstep_step_count = SEQ_MODEL_STEPS_PER_TRACK,
    k_quickstep_slot_count = SEQ_MODEL_VOICES_PER_STEP,
};

typedef struct {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    uint8_t armed;
} quickstep_cache_entry_t;

static CCM_DATA quickstep_cache_entry_t s_entries[k_quickstep_track_count][k_quickstep_step_count]
                                                            [k_quickstep_slot_count];
static uint8_t s_active_bank = 0xFFU;
static uint8_t s_active_pattern = 0xFFU;

static bool _valid_indices(uint8_t track, uint8_t step, uint8_t slot) {
    if (track >= k_quickstep_track_count) {
        return false;
    }
    if (step >= k_quickstep_step_count) {
        return false;
    }
    if (slot >= k_quickstep_slot_count) {
        return false;
    }
    return true;
}

static inline quickstep_cache_entry_t *_entry(uint8_t track, uint8_t step, uint8_t slot) {
    return &s_entries[track][step][slot];
}

static void _reset_entries(void) {
    memset(s_entries, 0, sizeof(s_entries));
}

void quickstep_cache_init(void) {
    s_active_bank = 0xFFU;
    s_active_pattern = 0xFFU;
    _reset_entries();
}

void quickstep_cache_set_active(uint8_t bank, uint8_t pattern) {
    if ((bank == s_active_bank) && (pattern == s_active_pattern)) {
        return;
    }

    s_active_bank = bank;
    s_active_pattern = pattern;
    _reset_entries();
}

void quickstep_cache_mark(uint8_t bank,
                          uint8_t pattern,
                          uint8_t track,
                          uint8_t step,
                          uint8_t slot,
                          uint8_t note,
                          uint8_t velocity,
                          uint8_t length) {
    if (!_valid_indices(track, step, slot)) {
        return;
    }

    quickstep_cache_set_active(bank, pattern);

    quickstep_cache_entry_t *entry = _entry(track, step, slot);
    entry->note = note;
    entry->velocity = (velocity == 0U) ? SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY : velocity;
    entry->length = (length == 0U) ? 1U : length;
    entry->armed = 1U;
}

bool quickstep_cache_fetch(uint8_t bank,
                           uint8_t pattern,
                           uint8_t track,
                           uint8_t step,
                           uint8_t slot,
                           uint8_t *out_note,
                           uint8_t *out_velocity,
                           uint8_t *out_length) {
    if ((bank != s_active_bank) || (pattern != s_active_pattern)) {
        return false;
    }
    if (!_valid_indices(track, step, slot)) {
        return false;
    }

    quickstep_cache_entry_t *entry = _entry(track, step, slot);
    if (entry->armed == 0U) {
        return false;
    }

    if (out_note != NULL) {
        *out_note = entry->note;
    }
    if (out_velocity != NULL) {
        *out_velocity = entry->velocity;
    }
    if (out_length != NULL) {
        *out_length = entry->length;
    }

    entry->armed = 0U;
    return true;
}

void quickstep_cache_disarm_step(uint8_t bank, uint8_t pattern, uint8_t track, uint8_t step) {
    if ((bank != s_active_bank) || (pattern != s_active_pattern)) {
        return;
    }
    if ((track >= k_quickstep_track_count) || (step >= k_quickstep_step_count)) {
        return;
    }

    for (uint8_t slot = 0U; slot < k_quickstep_slot_count; ++slot) {
        quickstep_cache_entry_t *entry = _entry(track, step, slot);
        entry->armed = 0U;
    }
}

