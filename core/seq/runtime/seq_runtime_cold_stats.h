/**
 * @file seq_runtime_cold_stats.h
 * @brief Host-only accounting helpers for cold runtime domains.
 */

#pragma once

#ifndef BRICK_SEQ_RUNTIME_COLD_STATS_H
#define BRICK_SEQ_RUNTIME_COLD_STATS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t bytes_project;
    size_t bytes_cart_meta;
    size_t bytes_hold_slots;
    size_t bytes_ui_shadow;
    size_t bytes_total;
} seq_cold_stats_t;

seq_cold_stats_t seq_runtime_cold_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_RUNTIME_COLD_STATS_H */
