/**
 * @file seq_runtime.c
 * @brief Shared sequencer runtime (project + active tracks).
 */

#include <string.h>

#include "brick_config.h"
#include "core/seq/seq_config.h"
#include "core/seq/seq_runtime.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_model.h"
#include "core/ram_audit.h"
#include "core/seq/runtime/seq_runtime_layout.h"

struct seq_runtime {
    seq_project_t     project;
    seq_model_track_t tracks[SEQ_RUNTIME_TRACK_CAPACITY];
};

seq_runtime_t g_seq_runtime;UI_RAM_AUDIT(g_seq_runtime);

static void _assign_virtual_carts(seq_project_t *project) {
    if (project == NULL) {
        return;
    }

    for (uint8_t cart = 0U; cart < SEQ_VIRTUAL_CART_COUNT; ++cart) {
        seq_project_cart_ref_t ref;
        memset(&ref, 0, sizeof(ref));
        ref.cart_id = SEQ_VIRTUAL_CART_UID(cart);
        ref.slot_id = cart;
        ref.capabilities = SEQ_PROJECT_CART_CAP_NONE;
        ref.flags = SEQ_PROJECT_CART_FLAG_NONE;

        const uint8_t base = (uint8_t)(cart * SEQ_VIRTUAL_TRACKS_PER_CART);
        for (uint8_t offset = 0U; offset < SEQ_VIRTUAL_TRACKS_PER_CART; ++offset) {
            const uint8_t track = (uint8_t)(base + offset);
            if ((track >= SEQ_RUNTIME_TRACK_CAPACITY) || (track >= SEQ_PROJECT_MAX_TRACKS)) {
                break;
            }
            seq_project_set_track_cart(project, track, &ref);
        }
    }
}

void seq_runtime_init(void) {
    seq_runtime_layout_reset_aliases();
    memset(&g_seq_runtime, 0, sizeof(g_seq_runtime));

    seq_project_init(&g_seq_runtime.project);

    for (uint8_t i = 0U; i < SEQ_RUNTIME_TRACK_CAPACITY; ++i) {
        seq_model_track_init(&g_seq_runtime.tracks[i]);
        seq_project_assign_track(&g_seq_runtime.project, i, &g_seq_runtime.tracks[i]);
    }

    _assign_virtual_carts(&g_seq_runtime.project);

    (void)seq_project_set_active_track(&g_seq_runtime.project, 0U);

    seq_runtime_layout_attach_aliases((const void*)&g_seq_runtime, (const void*)&g_seq_runtime);
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
