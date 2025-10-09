/**
 * @file drv_encoders.h
 * @brief Interface du driver des encodeurs rotatifs (TIM + soft quadrature).
 *
 * Fournit une API unifiée pour la lecture des 4 encodeurs :
 * - `ENC1` → TIM8 matériel
 * - `ENC2` → TIM4
 * - `ENC3` → Décodage quadrature logiciel (interruptions GPIO)
 * - `ENC4` → TIM2
 *
 * Fonctions principales :
 * - Lecture brute et remise à zéro
 * - Calcul d’incrément (delta) depuis le dernier appel
 * - Mode accéléré avec filtrage exponentiel (EMA) et détection de "flick"
 *
 * @note La configuration du matériel (TIM, GPIO, callbacks) est faite par `drv_encoders_start()`.
 *
 * @ingroup drivers
 */

#ifndef ENCODERS_H
#define ENCODERS_H

#include "ch.h"
#include "brick_config.h"
#include "hal.h"

/**
 * @enum encoder_id_t
 * @brief Identifiants des 4 encodeurs physiques.
 */
typedef enum {
    ENC1 = 0,  /**< Encodeur principal (TIM8)  */
    ENC2,      /**< Encodeur secondaire (TIM4) */
    ENC3,      /**< Encodeur logiciel GPIO     */
    ENC4       /**< Encodeur additionnel (TIM2) */
} encoder_id_t;

/* === API publique === */

/**
 * @brief Initialise le sous-système des encodeurs.
 *
 * Configure les timers matériels et les GPIO nécessaires,
 * ainsi que les interruptions pour l’encodeur logiciel (ENC3).
 */
void drv_encoders_start(void);

/**
 * @brief Retourne la valeur brute du compteur d’un encodeur.
 * @param id Identifiant de l’encodeur.
 * @return Valeur signée du compteur.
 */
int16_t drv_encoder_get(encoder_id_t id);

/**
 * @brief Réinitialise la position et l’état d’un encodeur.
 * @param id Identifiant de l’encodeur à réinitialiser.
 */
void drv_encoder_reset(encoder_id_t id);

/**
 * @brief Calcule l’incrément depuis le dernier appel (lecture brute).
 * @param id Identifiant de l’encodeur.
 * @return Incrément normalisé en “pas utilisateur”.
 */
int16_t drv_encoder_get_delta(encoder_id_t id);

/**
 * @brief Calcule l’incrément avec accélération dynamique (EMA + flick).
 *
 * Utilise un filtre exponentiel sur la vitesse et un système
 * d’impulsion temporaire lors des mouvements rapides pour améliorer
 * la réactivité.
 *
 * @param id Identifiant de l’encodeur.
 * @return Incrément accéléré normalisé en “pas utilisateur”.
 */
int16_t drv_encoder_get_delta_accel(encoder_id_t id);

#endif /* ENCODERS_H */
