/**
 * @file seq_runtime.h
 * @brief Shared sequencer runtime (project + active tracks).
 */

#ifndef BRICK_SEQ_RUNTIME_H
#define BRICK_SEQ_RUNTIME_H

#include <stdint.h>

#include "core/seq/seq_model.h"
#include "core/seq/seq_project.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SEQ_RUNTIME_TRACK_CAPACITY
#define SEQ_RUNTIME_TRACK_CAPACITY 2U
#endif

#ifndef SEQ_LED_BRIDGE_TRACK_CAPACITY
#define SEQ_LED_BRIDGE_TRACK_CAPACITY SEQ_RUNTIME_TRACK_CAPACITY
#endif

typedef struct {
    seq_project_t       project;
    seq_model_track_t   tracks[SEQ_RUNTIME_TRACK_CAPACITY];
} seq_runtime_t;

extern seq_runtime_t g_seq_runtime;

void seq_runtime_init(void);

const seq_project_t *seq_runtime_get_project(void);
seq_project_t *seq_runtime_access_project_mut(void);

const seq_model_track_t *seq_runtime_get_track(uint8_t idx);
seq_model_track_t *seq_runtime_access_track_mut(uint8_t idx);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_RUNTIME_H */
