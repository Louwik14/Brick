/**
 * @file seq_recorder.h
 * @brief Live recording bridge between the UI keyboard and seq_live_capture.
 */

#ifndef BRICK_APPS_SEQ_RECORDER_H
#define BRICK_APPS_SEQ_RECORDER_H

#include <stdint.h>
#include <stdbool.h>

#include "clock_manager.h"
#include "core/seq/seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void seq_recorder_init(seq_model_pattern_t *pattern);
void seq_recorder_attach_pattern(seq_model_pattern_t *pattern);
void seq_recorder_on_clock_step(const clock_step_info_t *info);
void seq_recorder_set_recording(bool enabled);
void seq_recorder_handle_note_on(uint8_t note, uint8_t velocity);
void seq_recorder_handle_note_off(uint8_t note);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_SEQ_RECORDER_H */
