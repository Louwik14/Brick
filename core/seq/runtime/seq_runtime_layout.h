#pragma once
#include <stdint.h>
#include <stddef.h>

// Budgets cibles (peuvent être ajustés plus tard, mais fixés pour la CI host)
#ifndef SEQ_RUNTIME_HOT_BUDGET_MAX
#define SEQ_RUNTIME_HOT_BUDGET_MAX (64u * 1024u)   // 64 KiB hot
#endif
#ifndef SEQ_RUNTIME_COLD_BUDGET_HINT
#define SEQ_RUNTIME_COLD_BUDGET_HINT (96u * 1024u) // indicatif
#endif

// Tags opaques pour les blocs
typedef struct { uint8_t _opaque; } seq_runtime_hot_t;
typedef struct { uint8_t _opaque_c; } seq_runtime_cold_t;

// Poignée d'accès interne (ne pas exposer aux apps)
typedef struct {
    // Nous ne stockons ici que des pointeurs vers les blocs RÉELS existants
    // (alias sur g_seq_runtime actuel) pour ne pas changer la BSS.
    const void* hot_impl;   // pointeur vers la zone "hot" actuelle (alias)
    const void* cold_impl;  // pointeur vers la zone "cold" actuelle (alias)
} seq_runtime_blocks_t;

// API interne core-only (pas pour apps/**)
#ifdef __cplusplus
extern "C" {
#endif

const seq_runtime_blocks_t* seq_runtime_blocks_get(void);

// API d'init interne (appelée depuis main/init runtime)
void seq_runtime_layout_reset_aliases(void);
void seq_runtime_layout_attach_aliases(const void* hot_impl, const void* cold_impl);

#ifdef __cplusplus
}
#endif
