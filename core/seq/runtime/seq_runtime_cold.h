/**
 * @file seq_runtime_cold.h
 * @brief Cold runtime façade returning readonly views into legacy data.
 */

#ifndef BRICK_SEQ_RUNTIME_COLD_H
#define BRICK_SEQ_RUNTIME_COLD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SEQ_COLDV_UI_SHADOW = 0,
    SEQ_COLDV_HOLD_SLOTS,
    SEQ_COLDV_PROJECT, /**< Legacy seq_project_t stored inside g_seq_runtime. */
    SEQ_COLDV__COUNT
} seq_cold_domain_t;

typedef struct {
    const void *_p;   /**< Pointer to the legacy storage backing the view. */
    size_t      _bytes; /**< Available byte length starting at @_p. */
} seq_cold_view_t;

/**
 * @brief Resolve a readonly legacy view for a cold runtime domain.
 *
 * The pointer and length are owned by the sequencer runtime and must not be
 * modified or cached across hot/cold relayout operations.
 */
seq_cold_view_t seq_runtime_cold_view(seq_cold_domain_t domain);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_RUNTIME_COLD_H */
