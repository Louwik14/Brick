#include "core/seq/runtime/seq_runtime_hot_budget.h"

#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_model.h"

// Local replica of the private Reader scratch state déclaré dans seq_reader.c (pool-only).
typedef struct {
    uint16_t base;
    uint8_t count;
    uint8_t index;
} seq_reader_plock_iter_state_sizeof_t;

enum {
    k_hot_reader_core = sizeof(seq_reader_plock_iter_state_sizeof_t),
    k_hot_scheduler_total = 0U,
    k_hot_scheduler_queue = 0U,
    k_hot_scheduler_core = 0U,
    k_hot_player_core = 0U,
    k_hot_reader_plock_iter = sizeof(seq_reader_plock_iter_state_sizeof_t),
    k_hot_player_stack_bytes = 0U,
    k_hot_rt_scratch = k_hot_reader_plock_iter,
    k_hot_total = k_hot_reader_core + k_hot_scheduler_core + k_hot_player_core +
                  k_hot_scheduler_queue + k_hot_rt_scratch
};

_Static_assert(k_hot_scheduler_total >= k_hot_scheduler_queue,
               "scheduler queue size exceeds scheduler total");
_Static_assert(k_hot_total <= SEQ_RUNTIME_HOT_BUDGET_MAX,
               "Hot runtime footprint exceeds budget");

seq_hot_snapshot_t seq_runtime_hot_snapshot(void) {
    seq_hot_snapshot_t snapshot = {0};
    snapshot.sizeof_reader_core = (size_t)k_hot_reader_core;
    snapshot.sizeof_scheduler_core = (size_t)k_hot_scheduler_core;
    snapshot.sizeof_player_core = (size_t)k_hot_player_core;
    snapshot.sizeof_rt_queues = (size_t)k_hot_scheduler_queue;
    snapshot.sizeof_rt_scratch = (size_t)k_hot_rt_scratch;
    return snapshot;
}

#if defined(HOST_BUILD) || defined(UNIT_TEST)
size_t __seq_runtime_hot_total_guard(void) {
    const seq_hot_snapshot_t snapshot = seq_runtime_hot_snapshot();
    return seq_runtime_hot_total(&snapshot);
}
#endif

