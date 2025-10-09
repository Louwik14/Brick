/**
 * @file ui_labels_common.h
 * @brief Déclarations des labels UI génériques et universels pour Brick.
 *
 * @ingroup ui
 *
 * @details
 * Ce header expose les ensembles de chaînes de caractères réutilisables
 * entre plusieurs modules ou cartouches du firmware Brick.
 *
 * Les labels définis ici ne concernent que les valeurs
 * **vraiment universelles** (booléens, formes d’onde de base, etc.),
 * et ne doivent jamais contenir de contenus spécifiques à un moteur DSP.
 *
 * L’objectif est d’offrir un référentiel commun pour limiter la duplication
 * de chaînes dans les fichiers `cart_xxx.c`.
 *
 * Exemple d’utilisation :
 * @code
 * #include "ui_labels_common.h"
 *
 * static const ui_param_spec_t PARAM_ENABLE = {
 *     .kind = UI_PARAM_BOOL,
 *     .meta.en = { ui_labels_onoff, 2 }
 * };
 * @endcode
 */

#ifndef BRICK_UI_UI_LABELS_COMMON_H
#define BRICK_UI_UI_LABELS_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Labels booléens / binaires
 * ============================================================ */

/**
 * @brief Labels génériques "Off" / "On".
 *
 * @details
 * Utilisés pour tout paramètre de type booléen :
 * activation d’un effet, bypass, sync LFO, etc.
 *
 * @code
 * .meta.en = { ui_labels_onoff, 2 }
 * @endcode
 */
extern const char* const ui_labels_onoff[2];

/* ============================================================
 * Labels de formes d’onde basiques
 * ============================================================ */

/**
 * @brief Formes d’onde de base communes à tous les moteurs.
 *
 * @details
 * Ordre des valeurs :
 * - "Sine"
 * - "Square"
 * - "Tri"
 * - "Saw"
 *
 * Utilisé pour les oscillateurs ou modulateurs génériques.
 *
 * @code
 * .meta.en = { ui_labels_basic_waves, 4 }
 * @endcode
 */
extern const char* const ui_labels_basic_waves[4];

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BRICK_UI_UI_LABELS_COMMON_H */
