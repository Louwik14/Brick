#pragma once

#include <stdint.h>

typedef enum {
  SEQ_RT_PHASE_BOOT = 0,
  SEQ_RT_PHASE_IDLE = 1,
  SEQ_RT_PHASE_TICK = 2
} seq_rt_phase_t;

#ifdef __cplusplus
extern "C" {
#endif

void seq_rt_phase_set(seq_rt_phase_t p);
seq_rt_phase_t seq_rt_phase_get(void);

#ifdef __cplusplus
}
#endif
