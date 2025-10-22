#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t step_abs;
    uint8_t trk;
    uint8_t slot;
    uint8_t note;
    uint8_t type;
} runner_trace_ev_t;

void runner_trace_reset(void);
void runner_trace_log(uint32_t step_abs,
                      uint8_t trk,
                      uint8_t slot,
                      uint8_t note,
                      uint8_t type);
size_t runner_trace_count(void);
const runner_trace_ev_t *runner_trace_get(size_t idx);

#ifdef __cplusplus
}
#endif

