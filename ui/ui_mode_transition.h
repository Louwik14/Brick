/**
 * @file ui_mode_transition.h
 * @brief Gestion des transitions de mode UI (SEQ/PMUTE/TRACK) et instrumentation.
 */

#ifndef BRICK_UI_MODE_TRANSITION_H
#define BRICK_UI_MODE_TRANSITION_H

#include <stdbool.h>
#include "ui_backend.h" /* seq_mode_t, ui_mode_context_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Trace macro utilisée pour diagnostiquer les transitions de mode.
 *
 * Activez la compilation avec `-DUI_DEBUG_TRACE_MODE_TRANSITION` pour obtenir
 * un log textuel sur stdout (en environnement host) ou la sortie console.
 */
#ifdef UI_DEBUG_TRACE_MODE_TRANSITION
#include <stdio.h>
#define UI_MODE_TRACE(fmt, ...) \
    do { printf("[ui-mode] " fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define UI_MODE_TRACE(fmt, ...) \
    do { (void)sizeof(fmt); } while (0)
#endif

/**
 * @brief Snapshot runtime d'une transition de mode.
 */
typedef struct {
    seq_mode_t previous_mode; /**< Mode avant transition. */
    seq_mode_t next_mode;     /**< Mode demandé. */
    const char *reason;       /**< Chaîne descriptive (optionnelle). */
    bool ui_synced;           /**< true si le contexte UI a été réinitialisé. */
    bool led_synced;          /**< true si les LEDs ont été synchronisées. */
    bool seq_synced;          /**< true si le bridge SEQ/engine a été synchronisé. */
} ui_mode_transition_t;

/**
 * @brief Initialise une transition de mode.
 */
void ui_mode_transition_begin(ui_mode_transition_t *transition,
                              seq_mode_t previous_mode,
                              seq_mode_t next_mode,
                              const char *reason);

/**
 * @brief Marque le reset du contexte UI comme effectué.
 */
void ui_mode_transition_mark_ui_synced(ui_mode_transition_t *transition);

/**
 * @brief Marque la synchronisation LED comme effectuée.
 */
void ui_mode_transition_mark_led_synced(ui_mode_transition_t *transition);

/**
 * @brief Marque la synchronisation séquenceur/engine comme effectuée.
 */
void ui_mode_transition_mark_seq_synced(ui_mode_transition_t *transition);

/**
 * @brief Finalise la transition et mémorise l'état courant (debug/tests).
 */
void ui_mode_transition_commit(const ui_mode_transition_t *transition);

/**
 * @brief Retourne le dernier snapshot de transition enregistré.
 */
const ui_mode_transition_t *ui_mode_transition_last(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_MODE_TRANSITION_H */
