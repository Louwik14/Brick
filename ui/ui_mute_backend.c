/**
 * @file ui_mute_backend.c
 * @brief Stub de backend MUTE (phase préparatoire avant lien avec séquenceur).
 * @ingroup ui
 *
 * @details
 * Ce fichier ne fait rien pour l’instant : il sert uniquement de
 * point d’entrée pour l’UI. Les fonctions seront implémentées côté
 * séquenceur (ex: seq_engine.c) lors de la Phase 6.
 */

#include "ui_mute_backend.h"
#include "brick_config.h"  /* pour éventuel DEBUG_ENABLE */
#include <stddef.h>

void ui_mute_backend_apply(uint8_t track, bool mute) {
#if DEBUG_ENABLE
    (void)track; (void)mute;
    /* debug_log("UI MUTE APPLY t%d=%d\n", track, mute); */
#endif
}

void ui_mute_backend_toggle_prepare(uint8_t track) {
#if DEBUG_ENABLE
    (void)track;
    /* debug_log("UI MUTE TOGGLE PREPARE t%d\n", track); */
#endif
}

void ui_mute_backend_commit(void) {
#if DEBUG_ENABLE
    /* debug_log("UI MUTE COMMIT\n"); */
#endif
}

void ui_mute_backend_cancel(void) {
#if DEBUG_ENABLE
    /* debug_log("UI MUTE CANCEL\n"); */
#endif
}
