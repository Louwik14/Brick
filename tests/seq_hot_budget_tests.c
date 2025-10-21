#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_engine.h"
#include "core/seq/seq_model.h"
#include "core/seq/seq_views.h"

static size_t hot_estimate_bytes(void) {
    size_t total = 0U;

    /*
     * Aggregated engine context (reader + scheduler + player).
     * Includes scheduler queue buffer and per-voice bookkeeping.
     */
    total += sizeof(seq_engine_t);

    /*
     * Reader plock iterator scratch (seq_reader.c, static).
     * Mirrors the legacy definition without exposing it publicly.
     */
    struct reader_plock_iter_state_sizeof {
        const seq_model_plock_t *plocks;
        uint8_t count;
        uint8_t index;
    };
    total += sizeof(struct reader_plock_iter_state_sizeof);

    /*
     * Player worker stack (THD_WORKING_AREA) declared in seq_engine.c.
     * ChibiOS stub maps THD_WORKING_AREA(size) to a raw uint8_t[size].
     */
    enum { k_player_stack_bytes = 768U };
    total += (size_t)k_player_stack_bytes;

    return total;
}

int main(void) {
    const size_t hot = hot_estimate_bytes();
    printf("HOT estimate (host): %zu bytes\n", hot);
    assert(hot <= SEQ_RUNTIME_HOT_BUDGET_MAX);
    return 0;
}
