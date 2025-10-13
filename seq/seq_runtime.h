/**
 * @file seq_runtime.h
 * @brief Lock-free publication of sequencer runtime snapshots for the UI.
 * @ingroup seq_runtime
 */
#ifndef BRICK_SEQ_RUNTIME_H
#define BRICK_SEQ_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>

#include "seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup seq_runtime Sequencer Runtime Snapshot
 *  @brief Immutable snapshots exposed to UI/LED layers.
 *  @{ */

typedef struct {
    bool            active;      /**< Step has at least one audible note. */
    bool            param_only;  /**< Step only holds parameters (no note). */
    seq_plock_mask_t plock_mask; /**< Locked parameter bit-mask. */
    uint8_t         note;        /**< Effective note after P-Lock/offset. */
    uint8_t         velocity;    /**< Effective velocity after offsets. */
    uint8_t         length;      /**< Effective length in steps. */
    int8_t          micro_timing;/**< Effective micro timing in ticks. */
    int16_t         params[SEQ_PARAM_COUNT]; /**< Exposed effective parameter values. */
} seq_runtime_step_t;

typedef struct {
    seq_runtime_step_t steps[SEQ_MODEL_STEP_COUNT]; /**< Per-step runtime state. */
    uint16_t           length;                     /**< Loop length for the voice. */
} seq_runtime_voice_t;

typedef struct {
    uint32_t            generation;   /**< Snapshot generation number. */
    uint32_t            playhead;     /**< Absolute playhead (0..). */
    seq_offsets_t       offsets;      /**< Offsets applied to all voices. */
    seq_runtime_voice_t voices[SEQ_MODEL_VOICE_COUNT]; /**< Voice snapshots. */
} seq_runtime_t;

/** Reset runtime buffers and prepare the publication mechanism. */
void seq_runtime_init(void);

/** Build a snapshot directly from the current pattern and playhead. */
void seq_runtime_snapshot_from_pattern(seq_runtime_t *dst,
                                       const seq_pattern_t *pattern,
                                       uint32_t playhead_index);

/** Publish a snapshot for lock-free consumption by the UI. */
void seq_runtime_publish(const seq_runtime_t *snapshot);

/** Retrieve the most recent immutable snapshot. */
const seq_runtime_t *seq_runtime_get_snapshot(void);

/** Check whether any voice emits a note at the given absolute step. */
bool seq_runtime_step_has_note(const seq_runtime_t *snapshot, uint32_t step_idx);

/** Tell whether at least one voice P-Locked the given parameter at that step. */
bool seq_runtime_step_param_is_plocked(const seq_runtime_t *snapshot,
                                       uint32_t step_idx,
                                       uint8_t param);

/** Return the effective parameter value (post offsets) for the requested step. */
int16_t seq_runtime_step_param_value(const seq_runtime_t *snapshot,
                                     uint32_t step_idx,
                                     uint8_t param);

/** Convenience getter exposing the absolute playhead index. */
uint32_t seq_runtime_playhead_index(const seq_runtime_t *snapshot);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_RUNTIME_H */
