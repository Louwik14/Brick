#include <assert.h>

#include "core/seq/runtime/seq_runtime_cold.h"
#include "core/seq/seq_runtime.h"

int main(void) {
    seq_runtime_init();

    seq_cold_view_t view = seq_runtime_cold_view(SEQ_COLDV_HOLD_SLOTS);
    assert(view._p != NULL);
    assert(view._bytes > 0U);

    return 0;
}
