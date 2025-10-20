#ifndef BRICK_APPS_SEQ_ENGINE_RUNNER_H
#define BRICK_APPS_SEQ_ENGINE_RUNNER_H

/**
 * @file seq_engine_runner.h
 * @brief Application-level bridge wiring the sequencer engine to MIDI/cart I/O.
 */

#include <stdbool.h>

#include "clock_manager.h"
#include "core/seq/seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void seq_engine_runner_init(seq_model_track_t *track);
void seq_engine_runner_attach_track(seq_model_track_t *track);
void seq_engine_runner_on_transport_play(void);
void seq_engine_runner_on_transport_stop(void);
void seq_engine_runner_on_clock_step(const clock_step_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_SEQ_ENGINE_RUNNER_H */
