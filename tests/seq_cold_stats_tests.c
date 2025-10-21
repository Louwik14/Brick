#include <assert.h>
#include <stdio.h>

#include "core/seq/runtime/seq_runtime_cold_stats.h"

int main(void) {
    seq_cold_stats_t stats = seq_runtime_cold_stats();

    printf("Cold domains (bytes): project=%zu cart_meta=%zu hold_slots=%zu ui_shadow=%zu total=%zu\n",
           stats.bytes_project,
           stats.bytes_cart_meta,
           stats.bytes_hold_slots,
           stats.bytes_ui_shadow,
           stats.bytes_total);

    assert(stats.bytes_total >= stats.bytes_project);
    return 0;
}
