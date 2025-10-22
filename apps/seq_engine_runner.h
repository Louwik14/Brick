#ifndef BRICK_APPS_SEQ_ENGINE_RUNNER_H
#define BRICK_APPS_SEQ_ENGINE_RUNNER_H

/**
 * @file seq_engine_runner.h
 * @brief Application-level bridge wiring the Reader-only runner to MIDI/cart I/O.
 */

#include <stdbool.h>

#include "clock_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

struct seq_model_track;
typedef struct seq_model_track seq_model_track_t;

void seq_engine_runner_init(seq_model_track_t *track);
void seq_engine_runner_attach_track(seq_model_track_t *track);
void seq_engine_runner_on_transport_play(void);
void seq_engine_runner_on_transport_stop(void);
void seq_engine_runner_on_clock_step(const clock_step_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_SEQ_ENGINE_RUNNER_H */
