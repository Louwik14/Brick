/**
 * @file seq_runtime.h
 * @brief Shared sequencer runtime (project + active tracks).
 */

#ifndef BRICK_SEQ_RUNTIME_H
#define BRICK_SEQ_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SEQ_RUNTIME_TRACK_CAPACITY
#define SEQ_RUNTIME_TRACK_CAPACITY 2U
#endif

#ifndef SEQ_LED_BRIDGE_TRACK_CAPACITY
#define SEQ_LED_BRIDGE_TRACK_CAPACITY SEQ_RUNTIME_TRACK_CAPACITY
#endif

typedef struct seq_project seq_project_t;
typedef struct seq_model_track seq_model_track_t;
typedef struct seq_runtime seq_runtime_t;

#ifndef SEQ_DEPRECATED
#if defined(__GNUC__) || defined(__clang__)
#define SEQ_DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define SEQ_DEPRECATED __declspec(deprecated)
#else
#define SEQ_DEPRECATED
#endif
#endif

extern seq_runtime_t g_seq_runtime;

void seq_runtime_init(void);

const seq_project_t *seq_runtime_get_project(void) SEQ_DEPRECATED;
seq_project_t *seq_runtime_access_project_mut(void) SEQ_DEPRECATED;

const seq_model_track_t *seq_runtime_get_track(uint8_t idx) SEQ_DEPRECATED;
seq_model_track_t *seq_runtime_access_track_mut(uint8_t idx) SEQ_DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_RUNTIME_H */
