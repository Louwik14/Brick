#include <assert.h>

#include "core/seq/runtime/seq_rt_phase.h"
#include "core/seq/runtime/seq_runtime_cold.h"

int main(void) {
    /* BOOT -> IDLE */
    seq_rt_phase_set(SEQ_RT_PHASE_IDLE);

    /* Simulate minimal tick: IDLE -> TICK -> IDLE */
    seq_rt_phase_set(SEQ_RT_PHASE_TICK);
    seq_rt_phase_set(SEQ_RT_PHASE_IDLE);

    assert(seq_rt_phase_get() == SEQ_RT_PHASE_IDLE);
    return 0;
}
