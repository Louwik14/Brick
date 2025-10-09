/**
 * @file drivers.c
 * @brief Point d’entrée unique pour l’initialisation de tous les drivers matériels du firmware.
 * @details Ce module centralise l’initialisation et la mise à jour périodique
 * de l’ensemble des pilotes bas niveau (affichage, entrées, LEDs, etc.).
 * Il sert de façade unique pour les couches supérieures (UI, moteur, etc.),
 * garantissant une séquence d’initialisation cohérente.
 *
 * @ingroup drivers
 */

#include "drivers.h"

/* ====================================================================== */
/*                         INITIALISATION GLOBALE                         */
/* ====================================================================== */

/**
 * @brief Initialise tous les périphériques matériels de Brick.
 *
 * Cette fonction est appelée une seule fois au démarrage du système,
 * avant l’initialisation de l’interface utilisateur et du moteur principal.
 *
 * Elle se charge notamment de :
 * - Initialiser l’écran OLED (affichage)
 * - Configurer les LEDs adressables
 * - Démarrer les threads de lecture des boutons, encodeurs et potentiomètres
 */
void drivers_init_all(void) {
    drv_display_init();
    drv_leds_addr_init();
    drv_buttons_start();
    drv_encoders_start();
    drv_pots_start();
}

/* ====================================================================== */
/*                          MISE À JOUR PÉRIODIQUE                        */
/* ====================================================================== */

/**
 * @brief Met à jour les drivers nécessitant un rafraîchissement périodique.
 *
 * À appeler régulièrement dans la boucle principale ou un thread système,
 * afin de maintenir les périphériques dynamiques (LEDs, écran, etc.) à jour.
 *
 * @note Cette fonction est optionnelle selon la configuration du firmware.
 */
void drivers_update_all(void) {
    drv_leds_addr_update();
    drv_display_update();   /**< Rafraîchissement manuel de l’écran si nécessaire. */
}
