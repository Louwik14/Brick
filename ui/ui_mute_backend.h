/**
 * @file ui_mute_backend.h
 * @brief Backend MUTE / PMUTE pour l'UI Brick (16 tracks, toggle, intégration LED).
 * @ingroup ui
 *
 * @details
 * Ce backend maintient l'état de MUTE pour 16 tracks (4 cartouches × 4 tracks)
 * et expose des opérations simples :
 * - apply(track, mute)    : fixe explicitement l'état d'une track
 * - toggle(track)         : bascule l'état (utilisé en QUICK MUTE à l'appui)
 * - toggle_prepare(track) : bascule l'état préparé (PMUTE), même rendu visuel
 * - commit() / cancel()   : applique/annule les préparations PMUTE
 *
 * Le rendu LED est assuré par @ref ui_led_backend (observateur passif). Ici on
 * publie uniquement les évènements nécessaires (MUTE_STATE / PMUTE_STATE).
 */

#ifndef BRICK_UI_MUTE_BACKEND_H
#define BRICK_UI_MUTE_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * API
 * ============================================================ */

/**
 * @brief Initialise l'état MUTE/PMUTE (toutes tracks démute, PMUTE vide).
 */
void ui_mute_backend_init(void);

/**
 * @brief Applique un état explicite sur une track.
 * @param track Index de la track (0..15)
 * @param mute  true = muté, false = actif
 *
 * Publie l'évènement @ref UI_LED_EVENT_MUTE_STATE vers le backend LED.
 */
void ui_mute_backend_apply(uint8_t track, bool mute);

/**
 * @brief Bascule l'état MUTE d'une track (toggle).
 * @param track Index de la track (0..15)
 *
 * Publie l'évènement @ref UI_LED_EVENT_MUTE_STATE vers le backend LED.
 */
void ui_mute_backend_toggle(uint8_t track);

/**
 * @brief Bascule l'état préparé PMUTE d'une track (toggle prepare).
 * @param track Index de la track (0..15)
 *
 * Publie @ref UI_LED_EVENT_PMUTE_STATE (même rendu visuel que MUTE).
 */
void ui_mute_backend_toggle_prepare(uint8_t track);

/**
 * @brief Valide toutes les préparations PMUTE (commit) et nettoie.
 *
 * Pour chaque track préparée, on inverse l'état MUTE réel et on publie
 * @ref UI_LED_EVENT_MUTE_STATE. Les flags préparés sont ensuite nettoyés
 * avec un @ref UI_LED_EVENT_PMUTE_STATE(false).
 */
void ui_mute_backend_commit(void);

/**
 * @brief Annule toutes les préparations PMUTE en cours (cancel).
 *
 * Publie @ref UI_LED_EVENT_PMUTE_STATE(false) pour chaque track préparée.
 */
void ui_mute_backend_cancel(void);

/**
 * @brief Republie l'état courant MUTE/PMUTE vers le backend LED.
 */
void ui_mute_backend_publish_state(void);

/**
 * @brief Purge toutes les préparations PMUTE sans toucher aux états MUTE réels.
 */
void ui_mute_backend_clear(void);

/* (Optionnel) Getters pour debug ou UI future : */
bool ui_mute_backend_is_muted(uint8_t track);
bool ui_mute_backend_is_prepared(uint8_t track);

#endif /* BRICK_UI_MUTE_BACKEND_H */
