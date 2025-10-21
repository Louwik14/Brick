#define _POSIX_C_SOURCE 199309L

#include "tests/support/rt_timing.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef RT_TIMING_SAMPLE_CAP
#define RT_TIMING_SAMPLE_CAP 512U
#endif

static uint64_t s_tick_begin_ns = 0U;
static uint64_t s_min_ns = 0U;
static uint64_t s_max_ns = 0U;
static double   s_sum_ns = 0.0;
static uint64_t s_count = 0U;
static uint64_t s_sample_ring[RT_TIMING_SAMPLE_CAP];
static size_t   s_sample_count = 0U;
static size_t   s_sample_index = 0U;

static uint64_t monotonic_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

void rt_tim_reset(void) {
    s_tick_begin_ns = 0U;
    s_min_ns = UINT64_MAX;
    s_max_ns = 0U;
    s_sum_ns = 0.0;
    s_count = 0U;
    s_sample_count = 0U;
    s_sample_index = 0U;
    memset(s_sample_ring, 0, sizeof(s_sample_ring));
}

void rt_tim_tick_begin(void) {
    s_tick_begin_ns = monotonic_now_ns();
}

void rt_tim_tick_end(void) {
    if (s_tick_begin_ns == 0U) {
        return;
    }

    const uint64_t end_ns = monotonic_now_ns();
    const uint64_t delta_ns = (end_ns >= s_tick_begin_ns) ? (end_ns - s_tick_begin_ns) : 0U;
    s_tick_begin_ns = 0U;

    if (s_count == 0U) {
        s_min_ns = delta_ns;
        s_max_ns = delta_ns;
    } else {
        if (delta_ns < s_min_ns) {
            s_min_ns = delta_ns;
        }
        if (delta_ns > s_max_ns) {
            s_max_ns = delta_ns;
        }
    }

    s_sum_ns += (double)delta_ns;
    ++s_count;

    s_sample_ring[s_sample_index % RT_TIMING_SAMPLE_CAP] = delta_ns;
    if (s_sample_count < RT_TIMING_SAMPLE_CAP) {
        ++s_sample_count;
    }
    ++s_sample_index;
}

static int cmp_u64(const void *a, const void *b) {
    const uint64_t va = *(const uint64_t *)a;
    const uint64_t vb = *(const uint64_t *)b;
    if (va < vb) {
        return -1;
    }
    if (va > vb) {
        return 1;
    }
    return 0;
}

static double percentile_ns(double fraction) {
    if (s_sample_count == 0U) {
        return 0.0;
    }

    uint64_t sorted[RT_TIMING_SAMPLE_CAP];
    memcpy(sorted, s_sample_ring, s_sample_count * sizeof(uint64_t));
    qsort(sorted, s_sample_count, sizeof(uint64_t), cmp_u64);

    const double rank = fraction * (double)(s_sample_count - 1U);
    const size_t idx_low = (size_t)rank;
    size_t idx_high = idx_low;
    if (idx_high + 1U < s_sample_count) {
        idx_high = idx_low + 1U;
    }

    if (idx_low == idx_high) {
        return (double)sorted[idx_low];
    }

    const double weight = rank - (double)idx_low;
    return ((1.0 - weight) * (double)sorted[idx_low]) + (weight * (double)sorted[idx_high]);
}

double rt_tim_p99_ns(void) {
    return percentile_ns(0.99);
}

void rt_tim_report(void) {
    const double avg_ns = (s_count > 0U) ? (s_sum_ns / (double)s_count) : 0.0;
    const double p95_ns = percentile_ns(0.95);
    const double p99_ns = percentile_ns(0.99);

    uint64_t min_ns = (s_count > 0U) ? s_min_ns : 0U;
    uint64_t max_ns = (s_count > 0U) ? s_max_ns : 0U;

    printf("tick_timing_ns: count=%" PRIu64 " min=%" PRIu64 " avg=%.0f p95=%.0f p99=%.0f max=%" PRIu64 "\n",
           s_count,
           min_ns,
           avg_ns,
           p95_ns,
           p99_ns,
           max_ns);
}
