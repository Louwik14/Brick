/**
 * @file metronome.h
 * @brief Interface du métronome interne.
 *
 * Fournit une tâche simple de clignotement ou de référence rythmique
 * basée sur un potentiomètre de tempo.
 * Peut être utilisée comme base pour synchroniser des éléments UI
 * ou générer une horloge MIDI interne.
 *
 * @ingroup system
 */
#ifndef METRONOME_H
#define METRONOME_H

/**
 * @brief Lance le thread du métronome interne.
 *
 * Le tempo est ajusté en fonction du potentiomètre #3 (PC3),
 * entre environ 60 et 240 BPM.
 */
void metronome_start(void);

#endif /* METRONOME_H */
