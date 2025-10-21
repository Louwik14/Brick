#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "core/seq/runtime/seq_rt_phase.h"
#include "core/seq/runtime/seq_runtime_cold.h"

int main(void) {
    seq_rt_phase_set(SEQ_RT_PHASE_IDLE);
    (void)seq_runtime_cold_view(SEQ_COLDV_PROJECT);

#if defined(HOST_BUILD) || defined(UNIT_TEST)
    extern unsigned __cold_view_calls_in_tick;
    bool caught = false;
    seq_rt_phase_set(SEQ_RT_PHASE_TICK);
    unsigned before = __cold_view_calls_in_tick;
    (void)seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    unsigned after = __cold_view_calls_in_tick;
    caught = (after == (before + 1U));
    assert(caught);
    printf("cold_view_calls_in_tick(host): %u\n", after);
#endif

    seq_rt_phase_set(SEQ_RT_PHASE_IDLE);
    return 0;
}
