#ifndef BRICK_CORE_SEQ_SEQ_PROJECT_H_
#define BRICK_CORE_SEQ_SEQ_PROJECT_H_

/**
 * @file seq_project.h
 * @brief Sequencer multi-track project container helpers.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of logical tracks a project can expose. */
#define SEQ_PROJECT_MAX_TRACKS 16U

/** Track descriptor stored by a project. */
typedef struct {
    seq_model_pattern_t *pattern; /**< Mutable pattern assigned to the track. */
} seq_project_track_t;

/** Sequencer project aggregating multiple tracks/patterns. */
typedef struct {
    seq_project_track_t tracks[SEQ_PROJECT_MAX_TRACKS]; /**< Track bindings. */
    uint8_t track_count;       /**< Highest contiguous track index bound. */
    uint8_t active_track;      /**< Currently selected track index. */
    seq_model_gen_t generation;/**< Generation bumped on topology changes. */
} seq_project_t;

void seq_project_init(seq_project_t *project);
bool seq_project_assign_track(seq_project_t *project, uint8_t track_index, seq_model_pattern_t *pattern);
seq_model_pattern_t *seq_project_get_track(seq_project_t *project, uint8_t track_index);
const seq_model_pattern_t *seq_project_get_track_const(const seq_project_t *project, uint8_t track_index);
bool seq_project_set_active_track(seq_project_t *project, uint8_t track_index);
uint8_t seq_project_get_active_track(const seq_project_t *project);
seq_model_pattern_t *seq_project_get_active_pattern(seq_project_t *project);
const seq_model_pattern_t *seq_project_get_active_pattern_const(const seq_project_t *project);
uint8_t seq_project_get_track_count(const seq_project_t *project);
void seq_project_clear_track(seq_project_t *project, uint8_t track_index);
void seq_project_bump_generation(seq_project_t *project);
const seq_model_gen_t *seq_project_get_generation(const seq_project_t *project);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CORE_SEQ_SEQ_PROJECT_H_ */
