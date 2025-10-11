/**
 * @file ui_mute_backend.h
 * @brief Interface neutre entre la couche UI et le moteur de séquenceur pour le mode MUTE.
 * @ingroup ui
 *
 * @details
 * Ce module est purement déclaratif : il ne contient pas de logique séquenceur.
 * Il expose des hooks appelés par la UI (Quick Mute / Prepare Mute),
 * implémentables plus tard dans le séquenceur sans dépendance circulaire.
 */

#ifndef BRICK_UI_MUTE_BACKEND_H
#define BRICK_UI_MUTE_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Active ou désactive immédiatement une piste du séquenceur (Quick Mute).
 * @param track Index de la piste (0..n-1).
 * @param mute  true pour mute, false pour unmute.
 */
void ui_mute_backend_apply(uint8_t track, bool mute);

/**
 * @brief Prépare une inversion d’état de mute (mode Prepare Mute).
 * @param track Index de la piste (0..n-1).
 */
void ui_mute_backend_toggle_prepare(uint8_t track);

/**
 * @brief Applique tous les mutes préparés (commit) et vide le buffer interne.
 */
void ui_mute_backend_commit(void);

/**
 * @brief Annule tous les mutes préparés (sortie sans commit).
 */
void ui_mute_backend_cancel(void);

#endif /* BRICK_UI_MUTE_BACKEND_H */
