#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "core/seq/seq_handles.h"
#include "core/seq/seq_views.h"

#ifdef __cplusplus
extern "C" {
#endif

bool seq_reader_get_step(seq_track_handle_t h, uint8_t step, seq_step_view_t *out);
bool seq_reader_get_step_voice(seq_track_handle_t h,
                               uint8_t step,
                               uint8_t voice_slot,
                               seq_step_voice_view_t *out);
bool seq_reader_count_step_voices(seq_track_handle_t h,
                                  uint8_t step,
                                  uint8_t *out_count);
bool seq_reader_plock_iter_open(seq_track_handle_t h, uint8_t step, seq_plock_iter_t *it);
bool seq_reader_plock_iter_next(seq_plock_iter_t *it, uint16_t *param_id, int32_t *value);

// MP3a: expose active track handle for apps
seq_track_handle_t seq_reader_get_active_track_handle(void);

static inline seq_track_handle_t seq_reader_make_handle(uint8_t bank, uint8_t pattern, uint8_t track) {
    seq_track_handle_t h = { bank, pattern, track };
    return h;
}

// --- P-Lock iteration (hybrid: legacy or pool) ------------------------------
struct seq_model_step_t;
typedef struct seq_model_step_t seq_model_step_t;

typedef struct {
  const seq_model_step_t *step;
  uint16_t off;
  uint8_t i;
  uint8_t n;
  uint8_t use_pool;
} seq_reader_pl_it_t;

int seq_reader_pl_open(seq_reader_pl_it_t *it, const seq_model_step_t *step);
int seq_reader_pl_next(seq_reader_pl_it_t *it, uint8_t *out_id, uint8_t *out_val, uint8_t *out_flags);

#ifdef __cplusplus
}
#endif
