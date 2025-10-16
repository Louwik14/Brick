/**
 * @file seq_project.c
 * @brief Sequencer multi-track project helpers implementation.
 */

#include "seq_project.h"

#include <string.h>

void seq_project_init(seq_project_t *project) {
    if (project == NULL) {
        return;
    }

    memset(project, 0, sizeof(*project));
    seq_model_gen_reset(&project->generation);
}

bool seq_project_assign_track(seq_project_t *project, uint8_t track_index, seq_model_pattern_t *pattern) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return false;
    }

    project->tracks[track_index].pattern = pattern;
    if ((pattern != NULL) && (track_index + 1U > project->track_count)) {
        project->track_count = (uint8_t)(track_index + 1U);
    }

    if ((project->active_track >= project->track_count) ||
        (project->tracks[project->active_track].pattern == NULL)) {
        project->active_track = track_index;
    }

    seq_project_bump_generation(project);
    return true;
}

seq_model_pattern_t *seq_project_get_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].pattern;
}

const seq_model_pattern_t *seq_project_get_track_const(const seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return NULL;
    }
    return project->tracks[track_index].pattern;
}

bool seq_project_set_active_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= project->track_count)) {
        return false;
    }
    if (project->tracks[track_index].pattern == NULL) {
        return false;
    }
    if (project->active_track == track_index) {
        return true;
    }

    project->active_track = track_index;
    seq_project_bump_generation(project);
    return true;
}

uint8_t seq_project_get_active_track(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    if (project->active_track >= project->track_count) {
        return 0U;
    }
    return project->active_track;
}

seq_model_pattern_t *seq_project_get_active_pattern(seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track(project, seq_project_get_active_track(project)) : NULL;
}

const seq_model_pattern_t *seq_project_get_active_pattern_const(const seq_project_t *project) {
    return (project != NULL) ? seq_project_get_track_const(project, seq_project_get_active_track(project)) : NULL;
}

uint8_t seq_project_get_track_count(const seq_project_t *project) {
    if (project == NULL) {
        return 0U;
    }
    return project->track_count;
}

void seq_project_clear_track(seq_project_t *project, uint8_t track_index) {
    if ((project == NULL) || (track_index >= SEQ_PROJECT_MAX_TRACKS)) {
        return;
    }

    project->tracks[track_index].pattern = NULL;
    while ((project->track_count > 0U) &&
           (project->tracks[project->track_count - 1U].pattern == NULL)) {
        project->track_count--;
    }

    if (project->active_track >= project->track_count) {
        project->active_track = 0U;
    }

    seq_project_bump_generation(project);
}

void seq_project_bump_generation(seq_project_t *project) {
    if (project == NULL) {
        return;
    }
    seq_model_gen_bump(&project->generation);
}

const seq_model_gen_t *seq_project_get_generation(const seq_project_t *project) {
    if (project == NULL) {
        return NULL;
    }
    return &project->generation;
}
