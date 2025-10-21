#include <assert.h>
#include <stdio.h>
#include "core/seq/runtime/seq_runtime_layout.h"

int main(void) {
    // Types opaques → tailles triviales (ne disent rien du layout réel)
    assert(sizeof(seq_runtime_hot_t)  >= 1);
    assert(sizeof(seq_runtime_cold_t) >= 1);

    // Vérif d'API : on peut obtenir les blocs
    const seq_runtime_blocks_t* b = seq_runtime_blocks_get();
    assert(b != NULL);
    // Pointeurs alias non-nuls par convention (dans notre bootstrap)
    assert(b->hot_impl != NULL);
    assert(b->cold_impl != NULL);

    // Reset/attach cycle : phase 1 -> phase 2
    const void* prev_hot = b->hot_impl;
    const void* prev_cold = b->cold_impl;
    seq_runtime_layout_reset_aliases();
    const seq_runtime_blocks_t* reset = seq_runtime_blocks_get();
    assert(reset->hot_impl == NULL);
    assert(reset->cold_impl == NULL);

    seq_runtime_layout_attach_aliases(prev_hot, prev_cold);
    const seq_runtime_blocks_t* attached = seq_runtime_blocks_get();
    assert(attached->hot_impl == prev_hot);
    assert(attached->cold_impl == prev_cold);

    // Budgets : on ne peut pas mesurer la vraie conso ici, mais on logge la cible
    printf("HOT budget max: %u\n", (unsigned)SEQ_RUNTIME_HOT_BUDGET_MAX);
    printf("COLD budget hint: %u\n", (unsigned)SEQ_RUNTIME_COLD_BUDGET_HINT);
    return 0;
}
