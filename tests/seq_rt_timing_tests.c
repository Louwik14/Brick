#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "core/seq/reader/seq_reader.h"
#include "core/seq/seq_runtime.h"

static uint64_t nsec_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

int main(void) {
    const int iters = 1000;
    seq_runtime_init();

    seq_track_handle_t handle = seq_reader_make_handle(0U, 0U, 0U);
    uint64_t t0 = nsec_now();
    for (int i = 0; i < iters; ++i) {
        seq_step_view_t view;
        (void)seq_reader_get_step(handle, (uint8_t)(i & 0x3F), &view);
    }
    uint64_t t1 = nsec_now();
    double avg_ns = (iters > 0) ? (double)(t1 - t0) / (double)iters : 0.0;
    printf("Reader.get_step: %g ns/call\n", avg_ns);

    /* TODO: add scheduler/player dry-run instrumentation if lightweight stubs are available. */
    return 0;
}
