/**
 * @file clock_manager.h
 * @brief Gestionnaire central d’horloge et de synchronisation tempo / MIDI.
 *
 * Ce module fournit une interface unifiée pour le contrôle du **tempo**, du **start/stop**
 * et du **routing MIDI Clock** :
 * - Horloge interne (générée localement)
 * - Horloge externe via MIDI (esclave)
 * - Conversion 24 PPQN → pas 1/16
 * - Callback d’avancement de pas **riche (V2)** pour le moteur de séquenceur
 *
 * @ingroup clock
 */

#ifndef CLOCK_MANAGER_H
#define CLOCK_MANAGER_H

#include <stdbool.h>
#include "ch.h"    // systime_t

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup clock Clock / Tempo
 *  @brief Gestion de l’horloge (interne/MIDI), conversion PPQN→steps, start/stop.
 *  @{
 */

/* ======================================================================
 *                               TYPES
 * ====================================================================== */

/**
 * @brief Source d’horloge (interne ou MIDI externe).
 */
typedef enum {
    CLOCK_SRC_INTERNAL,   /**< Horloge générée localement */
    CLOCK_SRC_MIDI        /**< Horloge reçue via MIDI Clock */
} clock_source_t;

/**
 * @brief Informations complètes passées au callback à chaque “step” (1/16).
 *
 * - `now` : horodatage absolu en `systime_t` (base ChibiOS)
 * - `step_idx_abs` : compteur de pas 1/16 depuis le start (peut déborder)
 * - `bpm` : tempo courant
 * - `tick_st` : durée d’1 tick MIDI (24 PPQN) en `systime_t`
 * - `step_st` : durée d’1 step (1/16 = 6 ticks) en `systime_t`
 * - `ext_clock` : true si la source active est une horloge externe
 */
typedef struct {
    systime_t now;
    uint32_t  step_idx_abs;
    float     bpm;
    systime_t tick_st;
    systime_t step_st;
    bool      ext_clock;
} clock_step_info_t;

/**
 * @brief Prototype du callback appelé à chaque step (V2).
 */
typedef void (*clock_step_cb2_t)(const clock_step_info_t *info);

/* ======================================================================
 *                               API
 * ====================================================================== */

/**
 * @brief Initialise le gestionnaire d’horloge.
 * @param src Source initiale (interne ou MIDI)
 */
void clock_manager_init(clock_source_t src);

/**
 * @brief Définit la source active de l’horloge.
 */
void clock_manager_set_source(clock_source_t src);

/**
 * @brief Retourne la source d’horloge actuellement active.
 */
clock_source_t clock_manager_get_source(void);

/**
 * @brief Définit le tempo (BPM) si l’horloge est interne.
 */
void clock_manager_set_bpm(float bpm);

/**
 * @brief Récupère le tempo courant (BPM).
 */
float clock_manager_get_bpm(void);

/**
 * @brief Démarre la génération d’horloge.
 * Envoie aussi `MIDI Start` sur la sortie active et réinitialise l’index de step.
 */
void clock_manager_start(void);

/**
 * @brief Arrête la génération d’horloge (et envoie `MIDI Stop`).
 */
void clock_manager_stop(void);

/**
 * @brief Indique si l’horloge est actuellement en cours d’exécution.
 */
bool clock_manager_is_running(void);

/**
 * @brief Enregistre un callback V2 appelé à chaque pas (1/16).
 * @param cb Pointeur vers la fonction callback (peut être NULL pour désinscrire).
 */
void clock_manager_register_step_callback2(clock_step_cb2_t cb);

/** @} */ // end of group clock

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_MANAGER_H */
