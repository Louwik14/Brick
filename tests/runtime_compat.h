#pragma once
#include <stdint.h>
#include "core/seq/seq_runtime.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static inline seq_project_t *seq_runtime_compat_access_project_mut(void) {
    return seq_runtime_access_project_mut();
}

static inline const seq_project_t *seq_runtime_compat_get_project(void) {
    return seq_runtime_get_project();
}

static inline seq_model_track_t *seq_runtime_compat_access_track_mut(uint8_t idx) {
    return seq_runtime_access_track_mut(idx);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
