/**
 * @file drv_buttons.h
 * @brief Interface du driver de lecture des boutons via registres à décalage **74HC165**.
 * @details
 * Ce module fournit l’accès haut niveau aux boutons physiques du système Brick.
 * Il encapsule la logique de lecture série des registres 74HC165 et publie les
 * événements de pression / relâchement sous forme asynchrone.
 *
 * Fonctions principales :
 * - Initialisation du driver et démarrage du thread de scan.
 * - Lecture instantanée de l’état d’un bouton.
 * - Récupération d’événements via mailbox (`polling`).
 *
 * @ingroup drivers
 */

#ifndef DRV_BUTTONS_H
#define DRV_BUTTONS_H

#include "ch.h"
#include "brick_config.h"
#include "hal.h"

/* ====================================================================== */
/*                              TYPES ET STRUCTURES                       */
/* ====================================================================== */

/**
 * @enum button_event_type_t
 * @brief Type d’événement de bouton détecté.
 */
typedef enum {
    BUTTON_EVENT_PRESS = 0,   /**< Pression détectée (front montant). */
    BUTTON_EVENT_RELEASE      /**< Relâchement détecté (front descendant). */
} button_event_type_t;

/**
 * @struct button_event_t
 * @brief Structure décrivant un événement de bouton.
 */
typedef struct {
    int id;                        /**< Identifiant du bouton (0–NUM_BUTTONS-1). */
    button_event_type_t type;      /**< Type d’événement (pression ou relâchement). */
} button_event_t;

/* ====================================================================== */
/*                              API PUBLIQUE                              */
/* ====================================================================== */

/**
 * @brief Initialise le driver des boutons et démarre le thread de lecture.
 *
 * Configure les lignes GPIO nécessaires et lance le thread
 * effectuant le scan périodique (~200 Hz).
 */
void drv_buttons_start(void);

/**
 * @brief Vérifie l’état courant d’un bouton.
 * @param id Identifiant du bouton à tester.
 * @return `true` si le bouton est actuellement pressé, sinon `false`.
 */
bool drv_button_is_pressed(int id);

/**
 * @brief Lit un événement de bouton dans la mailbox.
 *
 * @param[out] evt Structure où stocker l’événement lu.
 * @param[in] timeout Délai d’attente maximum (`TIME_IMMEDIATE`, `TIME_INFINITE`, etc.).
 * @return `true` si un événement a été lu avec succès, sinon `false` (timeout).
 */
bool drv_buttons_poll(button_event_t *evt, systime_t timeout);

#endif /* DRV_BUTTONS_H */
