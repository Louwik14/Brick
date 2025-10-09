/**
 * @file metronome.c
 * @brief Thread de métronome interne — tempo basé sur un potentiomètre.
 *
 * Ce module crée un thread simple qui clignote à la période correspondant
 * au tempo défini par un potentiomètre (PC3 / pot #3).
 * Il peut être étendu pour piloter une LED ou envoyer une clock MIDI.
 *
 * @ingroup system
 */

#include "ch.h"
#include "hal.h"
#include "drv_pots.h"

/* === Thread principal du métronome === */
static THD_WORKING_AREA(waMetronome, 256);
static THD_FUNCTION(metronomeThread, arg) {
    (void)arg;
    chRegSetThreadName("Metronome");

    bool led = false;

    while (true) {
        int bpm = 60 + (drv_pots_get(3) * 180 / 4095);  /* Potentiomètre → [60–240 BPM] */
        uint32_t period_ms = 60000 / bpm;               /* période d'une noire */

        /* TODO : clignoter LED, ou envoyer une clock MIDI */
        led = !led;
        (void)led;

        chThdSleepMilliseconds(period_ms);
    }
}

/**
 * @brief Démarre le thread du métronome interne.
 *
 * Le tempo est déterminé dynamiquement via le potentiomètre n°3.
 * Le thread tourne en continu (période = 1 temps à 4/4).
 */
void metronome_start(void) {
    chThdCreateStatic(waMetronome, sizeof(waMetronome),
                      NORMALPRIO, metronomeThread, NULL);
}
