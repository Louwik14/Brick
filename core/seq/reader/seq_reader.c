#include "core/seq/reader/seq_reader.h"

bool seq_reader_get_step(seq_track_handle_t h, uint8_t step, seq_step_view_t *out) {
  (void)h;
  (void)step;
  if (out != NULL) {
    out->note = 0U;
    out->vel = 0U;
    out->length = 0U;
    out->micro = 0;
    out->flags = 0U;
  }
  return false;
}

bool seq_reader_plock_iter_open(seq_track_handle_t h, uint8_t step, seq_plock_iter_t *it) {
  (void)h;
  (void)step;
  if (it != NULL) {
    it->_opaque = NULL;
  }
  return false;
}

bool seq_reader_plock_iter_next(seq_plock_iter_t *it, uint16_t *param_id, int32_t *value) {
  (void)it;
  if (param_id != NULL) {
    *param_id = 0U;
  }
  if (value != NULL) {
    *value = 0;
  }
  return false;
}
