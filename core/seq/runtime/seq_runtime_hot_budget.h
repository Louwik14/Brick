#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t sizeof_reader_core;
    size_t sizeof_scheduler_core;
    size_t sizeof_player_core;
    size_t sizeof_rt_queues;
    size_t sizeof_rt_scratch;
} seq_hot_snapshot_t;

seq_hot_snapshot_t seq_runtime_hot_snapshot(void);

static inline size_t seq_runtime_hot_total(const seq_hot_snapshot_t *s) {
    return s->sizeof_reader_core + s->sizeof_scheduler_core +
           s->sizeof_player_core + s->sizeof_rt_queues +
           s->sizeof_rt_scratch;
}

#if defined(HOST_BUILD) || defined(UNIT_TEST)
size_t __seq_runtime_hot_total_guard(void);
#endif

#ifdef __cplusplus
}
#endif

