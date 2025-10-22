/**
 * @file seq_recorder.h
 * @brief Live recording bridge between the UI keyboard and seq_live_capture.
 */

#ifndef BRICK_APPS_SEQ_RECORDER_H
#define BRICK_APPS_SEQ_RECORDER_H

#include <stdint.h>
#include <stdbool.h>

#include "clock_manager.h"
#include "apps/rtos_shim.h" // --- ARP FIX: timestamp explicite ---
#include "core/seq/seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void seq_recorder_init(seq_model_track_t *track);
void seq_recorder_attach_track(seq_model_track_t *track);
void seq_recorder_on_clock_step(const clock_step_info_t *info);
void seq_recorder_set_recording(bool enabled);
void seq_recorder_handle_note_on(uint8_t note, uint8_t velocity);
void seq_recorder_handle_note_on_at(uint8_t note, uint8_t velocity, systime_t timestamp); // --- ARP FIX: batch timestamp ---
void seq_recorder_handle_note_off(uint8_t note);
void seq_recorder_handle_note_off_at(uint8_t note, systime_t timestamp); // --- ARP FIX: batch timestamp ---

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_SEQ_RECORDER_H */
