#include "apps/runner_trace.h"

#ifndef RUNNER_TRACE_CAP
#define RUNNER_TRACE_CAP 256u
#endif

static runner_trace_ev_t s_ring[RUNNER_TRACE_CAP];
static size_t s_head = 0u;
static size_t s_size = 0u;

void runner_trace_reset(void) {
    s_head = 0u;
    s_size = 0u;
}

void runner_trace_log(uint32_t step_abs,
                      uint8_t trk,
                      uint8_t slot,
                      uint8_t note,
                      uint8_t type) {
    const size_t tail = (s_head + s_size) % RUNNER_TRACE_CAP;
    s_ring[tail].step_abs = step_abs;
    s_ring[tail].trk = trk;
    s_ring[tail].slot = slot;
    s_ring[tail].note = note;
    s_ring[tail].type = type;

    if (s_size < RUNNER_TRACE_CAP) {
        ++s_size;
    } else {
        s_head = (s_head + 1u) % RUNNER_TRACE_CAP;
    }
}

size_t runner_trace_count(void) {
    return s_size;
}

const runner_trace_ev_t *runner_trace_get(size_t idx) {
    if (idx >= s_size) {
        return NULL;
    }

    const size_t pos = (s_head + idx) % RUNNER_TRACE_CAP;
    return &s_ring[pos];
}

