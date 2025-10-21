#ifndef BRICK_APPS_SEQ_ENGINE_RUNNER_H
#define BRICK_APPS_SEQ_ENGINE_RUNNER_H

#include <stdint.h>

#include "clock_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

void seq_engine_runner_init(void);
void seq_engine_runner_on_transport_play(void);
void seq_engine_runner_on_transport_stop(void);
void seq_engine_runner_on_clock_step(const clock_step_info_t *info);
void seq_runner_set_active_pattern(uint8_t bank, uint8_t pattern);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_APPS_SEQ_ENGINE_RUNNER_H */
