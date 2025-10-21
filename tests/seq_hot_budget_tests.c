#include <assert.h>
#include <stdio.h>

#include "core/seq/runtime/seq_runtime_hot_budget.h"
#include "core/seq/runtime/seq_runtime_layout.h"

int main(void) {
    const seq_hot_snapshot_t snapshot = seq_runtime_hot_snapshot();
    const size_t hot = seq_runtime_hot_total(&snapshot);

    printf("HOT detail:\n  reader=%zu, scheduler=%zu, player=%zu, queues=%zu, scratch=%zu\n",
           snapshot.sizeof_reader_core, snapshot.sizeof_scheduler_core,
           snapshot.sizeof_player_core, snapshot.sizeof_rt_queues,
           snapshot.sizeof_rt_scratch);
    printf("HOT estimate (host): %zu bytes\n", hot);

    assert(hot <= SEQ_RUNTIME_HOT_BUDGET_MAX);

#if defined(HOST_BUILD) || defined(UNIT_TEST)
    /* Force link of the compile-time guard to surface violations during host builds. */
    (void)__seq_runtime_hot_total_guard();
#endif

    return 0;
}
