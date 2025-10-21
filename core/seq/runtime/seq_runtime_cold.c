#include "core/seq/runtime/seq_runtime_cold.h"

#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

#if defined(HOST_BUILD) || defined(UNIT_TEST)
#include <assert.h>
#include "core/seq/runtime/seq_rt_phase.h"

unsigned __cold_view_calls_in_tick = 0u;
#endif

typedef struct {
    const void *_p;
    size_t _bytes;
} _cv;

extern uint8_t g_hold_slots[];
extern const size_t g_hold_slots_size;

typedef struct {
    seq_project_t     project;
    seq_model_track_t tracks[SEQ_RUNTIME_TRACK_CAPACITY];
} seq_runtime_legacy_t;

static _cv _resolve(seq_cold_domain_t domain) {
    switch (domain) {
        case SEQ_COLDV_PROJECT: {
            const seq_runtime_blocks_t *blocks = seq_runtime_blocks_get();
            if ((blocks == NULL) || (blocks->cold_impl == NULL)) {
                return (_cv){ NULL, 0U };
            }

            const seq_runtime_legacy_t *legacy =
                (const seq_runtime_legacy_t *)blocks->cold_impl;
            return (_cv){ (const void *)&legacy->project,
                          sizeof(legacy->project) };
        }
        case SEQ_COLDV_HOLD_SLOTS:
            if (g_hold_slots_size > 0U) {
                return (_cv){ (const void *)g_hold_slots, g_hold_slots_size };
            }
            return (_cv){ NULL, 0U };
        case SEQ_COLDV_CART_META: {
            const seq_runtime_blocks_t *blocks = seq_runtime_blocks_get();
            if ((blocks == NULL) || (blocks->cold_impl == NULL)) {
                return (_cv){ NULL, 0U };
            }

            const seq_runtime_legacy_t *legacy =
                (const seq_runtime_legacy_t *)blocks->cold_impl;
            return (_cv){ (const void *)legacy->project.tracks,
                          sizeof(legacy->project.tracks) };
        }
        case SEQ_COLDV_UI_SHADOW:
        default:
            return (_cv){ NULL, 0U };
    }
}

seq_cold_view_t seq_runtime_cold_view(seq_cold_domain_t domain) {
#if defined(HOST_BUILD) || defined(UNIT_TEST)
    if (seq_rt_phase_get() == SEQ_RT_PHASE_TICK) {
        __cold_view_calls_in_tick++;
#if !defined(UNIT_TEST)
        assert(0 && "cold view access during RT tick");
#endif
    }
#endif
    _cv raw = _resolve(domain);
    seq_cold_view_t view = { raw._p, raw._bytes };
    return view;
}
