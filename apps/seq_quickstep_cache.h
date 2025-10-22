#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    bool valid;
} seq_quickstep_cache_entry_t;

void seq_quickstep_cache_init(void);
void seq_quickstep_cache_mark(uint8_t track, uint8_t step_idx, uint8_t slot,
                              uint8_t note, uint8_t velocity, uint8_t length);
void seq_quickstep_cache_invalidate(uint8_t track, uint8_t step_idx, uint8_t slot);
bool seq_quickstep_cache_consume(uint8_t track, uint8_t step_idx, uint8_t slot,
                                 seq_quickstep_cache_entry_t *out_entry);

#ifdef __cplusplus
}
#endif

