#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void quickstep_cache_init(void);
void quickstep_cache_set_active(uint8_t bank, uint8_t pattern);
void quickstep_cache_mark(uint8_t bank,
                          uint8_t pattern,
                          uint8_t track,
                          uint8_t step,
                          uint8_t slot,
                          uint8_t note,
                          uint8_t velocity,
                          uint8_t length);
bool quickstep_cache_fetch(uint8_t bank,
                           uint8_t pattern,
                           uint8_t track,
                           uint8_t step,
                           uint8_t slot,
                           uint8_t *out_note,
                           uint8_t *out_velocity,
                           uint8_t *out_length);
void quickstep_cache_disarm_step(uint8_t bank, uint8_t pattern, uint8_t track, uint8_t step);

#ifdef __cplusplus
}
#endif

