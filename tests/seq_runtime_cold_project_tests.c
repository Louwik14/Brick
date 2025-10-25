#include <assert.h>

#include "core/seq/runtime/seq_runtime_cold.h"
#include "core/seq/seq_project.h"
#include "core/seq/seq_runtime.h"
#include "tests/runtime_compat.h"

int main(void) {
    seq_runtime_init();

    seq_cold_view_t view = seq_runtime_cold_view(SEQ_COLDV_PROJECT);
    assert(view._p != NULL);
    assert(view._bytes >= sizeof(seq_project_t));

    const seq_project_t *project = (const seq_project_t *)view._p;
    assert(project == seq_runtime_compat_get_project());

    return 0;
}
