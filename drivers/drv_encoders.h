/**
 * @file drv_encoders.h
 * @brief Interface du driver des encodeurs rotatifs.
 *
 * Le driver regroupe l’ensemble des stratégies matérielles employées sur Brick :
 * - encodeurs quadrature matériel (TIMx en mode encoder) ;
 * - quadrature logiciel via interruptions GPIO ;
 * - entrées "step/dir" sur ligne `EXT`.
 *
 * L’API expose une lecture normalisée en *pas utilisateur* (1 par cran) et un
 * mode accéléré basé sur un EMA de vitesse avec seuils/hystérésis.
 *
 * @note La configuration matérielle (TIM, GPIO, callbacks) est entièrement
 * gérée par `drv_encoders_start()` ; aucune dépendance externe n’est requise.
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
 * @brief Réinitialise la position et l’état interne d’un encodeur.
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
 * @brief Calcule l’incrément avec accélération dynamique (EMA + hystérésis).
 *
 * La vitesse instantanée est filtrée par EMA et comparée à des seuils
 * configurables (avec hystérésis) afin de sélectionner le multiplicateur
 * d’accélération.
 *
 * @param id Identifiant de l’encodeur.
 * @return Incrément accéléré normalisé en “pas utilisateur”.
 */
int16_t drv_encoder_get_delta_accel(encoder_id_t id);

#endif /* ENCODERS_H */
