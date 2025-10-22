#include "apps/seq_quickstep_cache.h"

#include <string.h>

#include "brick_config.h"
#include "core/seq/seq_access.h"

typedef struct {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    uint8_t valid;
} seq_quickstep_cache_cell_t;

static CCM_DATA seq_quickstep_cache_cell_t s_cache[SEQ_PROJECT_MAX_TRACKS]
                                               [SEQ_MODEL_STEPS_PER_TRACK]
                                               [SEQ_MODEL_VOICES_PER_STEP];

void seq_quickstep_cache_init(void) {
    memset(s_cache, 0, sizeof(s_cache));
}

static bool _range_ok(uint8_t track, uint8_t step_idx, uint8_t slot) {
    if (track >= SEQ_PROJECT_MAX_TRACKS) {
        return false;
    }
    if (step_idx >= SEQ_MODEL_STEPS_PER_TRACK) {
        return false;
    }
    if (slot >= SEQ_MODEL_VOICES_PER_STEP) {
        return false;
    }
    return true;
}

void seq_quickstep_cache_mark(uint8_t track, uint8_t step_idx, uint8_t slot,
                              uint8_t note, uint8_t velocity, uint8_t length) {
    if (!_range_ok(track, step_idx, slot)) {
        return;
    }

    seq_quickstep_cache_cell_t *cell = &s_cache[track][step_idx][slot];
    cell->note = note;
    cell->velocity = velocity;
    cell->length = length;
    cell->valid = 1U;
}

void seq_quickstep_cache_invalidate(uint8_t track, uint8_t step_idx, uint8_t slot) {
    if (!_range_ok(track, step_idx, slot)) {
        return;
    }
    s_cache[track][step_idx][slot].valid = 0U;
}

bool seq_quickstep_cache_consume(uint8_t track, uint8_t step_idx, uint8_t slot,
                                 seq_quickstep_cache_entry_t *out_entry) {
    if (!_range_ok(track, step_idx, slot)) {
        return false;
    }

    seq_quickstep_cache_cell_t *cell = &s_cache[track][step_idx][slot];
    if (cell->valid == 0U) {
        return false;
    }

    if (out_entry != NULL) {
        out_entry->note = cell->note;
        out_entry->velocity = cell->velocity;
        out_entry->length = cell->length;
        out_entry->valid = true;
    }

    cell->valid = 0U;
    return true;
}

