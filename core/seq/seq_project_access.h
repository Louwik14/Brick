#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct seq_project;
typedef struct seq_project seq_project_t;

uint8_t seq_project_get_cart_count(const seq_project_t *project);
const char* seq_project_get_cart_name(const seq_project_t *project, uint8_t cart_index);
bool seq_project_get_cart_track_span(const seq_project_t *project,
                                     uint8_t cart_index,
                                     uint16_t *start_track,
                                     uint16_t *track_count);
uint16_t seq_project_get_track_count(const seq_project_t *project);

#ifdef __cplusplus
}
#endif
