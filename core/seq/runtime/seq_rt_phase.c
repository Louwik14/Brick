#include "core/seq/runtime/seq_rt_phase.h"

static volatile seq_rt_phase_t s_phase = SEQ_RT_PHASE_BOOT;

void seq_rt_phase_set(seq_rt_phase_t p) {
    s_phase = p;
}

seq_rt_phase_t seq_rt_phase_get(void) {
    return s_phase;
}
