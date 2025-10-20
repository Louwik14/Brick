/**
 * @file seq_runtime.c
 * @brief Shared sequencer runtime (project + active tracks).
 */

#include <string.h>

#include "brick_config.h"
#include "core/seq/seq_runtime.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_model.h"
#include "core/ram_audit.h"

struct seq_runtime {
    seq_project_t     project;
    seq_model_track_t tracks[SEQ_RUNTIME_TRACK_CAPACITY];
};

seq_runtime_t g_seq_runtime;UI_RAM_AUDIT(g_seq_runtime);

void seq_runtime_init(void) {
    memset(&g_seq_runtime, 0, sizeof(g_seq_runtime));

    seq_project_init(&g_seq_runtime.project);

    for (uint8_t i = 0U; i < SEQ_RUNTIME_TRACK_CAPACITY; ++i) {
        seq_model_track_init(&g_seq_runtime.tracks[i]);
        seq_project_assign_track(&g_seq_runtime.project, i, &g_seq_runtime.tracks[i]);
    }

    (void)seq_project_set_active_track(&g_seq_runtime.project, 0U);
}

const seq_project_t *seq_runtime_get_project(void) {
    return &g_seq_runtime.project;
}

seq_project_t *seq_runtime_access_project_mut(void) {
    return &g_seq_runtime.project;
}

const seq_model_track_t *seq_runtime_get_track(uint8_t idx) {
    if (idx >= SEQ_RUNTIME_TRACK_CAPACITY) {
        return NULL;
    }
    return &g_seq_runtime.tracks[idx];
}

seq_model_track_t *seq_runtime_access_track_mut(uint8_t idx) {
    if (idx >= SEQ_RUNTIME_TRACK_CAPACITY) {
        return NULL;
    }
    return &g_seq_runtime.tracks[idx];
}
