/**
 * @file drv_pots.h
 * @brief Interface du driver pour les potentiomètres analogiques (ADC1 : PC0–PC3).
 *
 * Ce module gère la lecture continue des entrées analogiques connectées aux potentiomètres.
 * - Acquisition par **ADC1** sur les canaux IN10 à IN13
 * - Moyennage logiciel sur plusieurs échantillons
 * - Thread de lecture à fréquence fixe (~50 Hz)
 *
 * @note Les GPIOs PC0–PC3 doivent être configurés en mode analogique avant utilisation.
 *
 * @ingroup drivers
 */

#ifndef DRV_POTS_H
#define DRV_POTS_H

#include "ch.h"
#include "brick_config.h"
#include "hal.h"

/* =======================================================================
 *                              API PUBLIQUE
 * ======================================================================= */

/**
 * @brief Initialise le sous-système des potentiomètres.
 *
 * Configure les entrées ADC nécessaires (PC0–PC3).
 * Ne lance pas encore la conversion continue.
 */
void drv_pots_init(void);

/**
 * @brief Démarre le thread de lecture et de moyennage des potentiomètres.
 *
 * Lance une conversion continue via ADC1 et met à jour
 * les valeurs internes toutes les 20 ms environ.
 */
void drv_pots_start(void);

/**
 * @brief Récupère la valeur actuelle d’un potentiomètre.
 * @param index Indice du potentiomètre [0 – NUM_POTS – 1].
 * @return Valeur ADC moyenne (0 – 4095).
 */
int drv_pots_get(int index);

#endif /* DRV_POTS_H */
