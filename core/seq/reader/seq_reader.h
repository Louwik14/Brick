#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/seq/seq_handles.h"
#include "core/seq/seq_views.h"

#ifdef __cplusplus
extern "C" {
#endif

bool seq_reader_get_step(seq_track_handle_t h, uint8_t step, seq_step_view_t *out);
bool seq_reader_plock_iter_open(seq_track_handle_t h, uint8_t step, seq_plock_iter_t *it);
bool seq_reader_plock_iter_next(seq_plock_iter_t *it, uint16_t *param_id, int32_t *value);

// MP3a: expose active track handle for apps
seq_track_handle_t seq_reader_get_active_track_handle(void);

static inline seq_track_handle_t seq_reader_make_handle(uint8_t bank, uint8_t pattern, uint8_t track) {
    seq_track_handle_t h = { bank, pattern, track };
    return h;
}

#ifdef __cplusplus
}
#endif
