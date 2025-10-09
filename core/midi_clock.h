/**
 * @file midi_clock.h
 * @brief Interface du générateur d’horloge MIDI (24 PPQN via GPT3).
 *
 * Ce module fournit une horloge MIDI conforme à la spécification :
 * - Envoi automatique de messages `0xF8` à fréquence dépendant du BPM.
 * - Basée sur le timer matériel **TIM3** configuré à 1 MHz.
 * - Résolution temporelle : **1 µs**.
 * - Prise en charge du **callback applicatif** à chaque tick (24 PPQN).
 * - Support du démarrage/arrêt dynamique et ajustement de tempo en temps réel.
 *
 * @ingroup midi
 */

#ifndef MIDI_CLOCK_H
#define MIDI_CLOCK_H

#include "ch.h"
#include "hal.h"
#include "midi.h"   /* pour midi_clock(MIDI_DEST_BOTH) */

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================== */
/*                              TYPES                                     */
/* ====================================================================== */

/**
 * @typedef midi_tick_cb_t
 * @brief Type de callback appelé à chaque tick MIDI (24 PPQN).
 */
typedef void (*midi_tick_cb_t)(void);

/* ====================================================================== */
/*                              API PUBLIQUE                              */
/* ====================================================================== */

/**
 * @brief Enregistre un callback appelé à chaque tick (24 PPQN).
 * @param cb Pointeur de fonction callback
 */
void  midi_clock_register_tick_callback(midi_tick_cb_t cb);

/**
 * @brief Initialise le générateur d’horloge MIDI.
 *
 * Configure GPT3 (1 MHz), initialise la sémaphore de synchronisation
 * et crée le thread d’envoi des ticks.
 */
void  midi_clock_init(void);

/**
 * @brief Démarre la génération de MIDI Clock.
 *
 * Lance le GPT en mode continu selon le BPM courant.
 */
void  midi_clock_start(void);

/**
 * @brief Stoppe la génération de MIDI Clock.
 *
 * Arrête le timer GPT (plus de tick F8).
 */
void  midi_clock_stop(void);

/**
 * @brief Définit le BPM et recalcule l’intervalle du timer.
 *
 * Si la clock est active, le GPT est redémarré avec la nouvelle période.
 * @param bpm Nouveau tempo en battements par minute.
 */
void  midi_clock_set_bpm(float bpm);

/**
 * @brief Retourne le tempo actuel (BPM).
 */
float midi_clock_get_bpm(void);

/**
 * @brief Indique si l’horloge MIDI est actuellement en cours d’exécution.
 * @return true si la clock tourne, false sinon.
 */
bool  midi_clock_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* MIDI_CLOCK_H */
