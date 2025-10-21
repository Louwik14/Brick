#include "core/seq/runtime/seq_runtime_cold.h"

#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"

typedef struct {
    const void *_p;
    size_t _bytes;
} _cv;

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
        case SEQ_COLDV_UI_SHADOW:
        case SEQ_COLDV_HOLD_SLOTS:
        default:
            return (_cv){ NULL, 0U };
    }
}

seq_cold_view_t seq_runtime_cold_view(seq_cold_domain_t domain) {
    _cv raw = _resolve(domain);
    seq_cold_view_t view = { raw._p, raw._bytes };
    return view;
}
