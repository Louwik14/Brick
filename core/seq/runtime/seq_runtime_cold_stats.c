#include "core/seq/runtime/seq_runtime_cold_stats.h"

#include "core/seq/runtime/seq_runtime_cold.h"

seq_cold_stats_t seq_runtime_cold_stats(void) {
    seq_cold_stats_t stats = {0U, 0U, 0U, 0U, 0U};

    seq_cold_view_t view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    stats.bytes_project = view._bytes;

    view = seq_runtime_cold_view(SEQ_COLDV_CART_META);
    stats.bytes_cart_meta = view._bytes;

    view = seq_runtime_cold_view(SEQ_COLDV_HOLD_SLOTS);
    stats.bytes_hold_slots = view._bytes;

    view = seq_runtime_cold_view(SEQ_COLDV_UI_SHADOW);
    stats.bytes_ui_shadow = view._bytes;

    stats.bytes_total = stats.bytes_project + stats.bytes_cart_meta +
                        stats.bytes_hold_slots + stats.bytes_ui_shadow;
    return stats;
}
