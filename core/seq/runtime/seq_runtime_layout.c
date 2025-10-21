#include "core/seq/runtime/seq_runtime_layout.h"
#include "core/seq/seq_runtime.h"   // runtime legacy existant

static seq_runtime_blocks_t s_blocks = {0};

__attribute__((constructor)) static void _seq_runtime_blocks_bootstrap(void) {
    // Alias interne : on pointe vers les structures actuelles (ex: &g_seq_runtime)
    // sans modifier leur layout ni leur placement.
    s_blocks.hot_impl  = (const void*)&g_seq_runtime;  // alias provisoire
    s_blocks.cold_impl = (const void*)&g_seq_runtime;  // alias provisoire
}

const seq_runtime_blocks_t* seq_runtime_blocks_get(void) {
    return &s_blocks;
}

void seq_runtime_layout_reset_aliases(void) {
    s_blocks.hot_impl  = NULL;
    s_blocks.cold_impl = NULL;
}

void seq_runtime_layout_attach_aliases(const void* hot_impl, const void* cold_impl) {
    s_blocks.hot_impl  = hot_impl;
    s_blocks.cold_impl = cold_impl;
}
