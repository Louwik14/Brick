#ifndef BRICK_UI_UI_SHORTCUTS_H
#define BRICK_UI_UI_SHORTCUTS_H
/**
 * @file ui_shortcuts.h
 * @brief Détection centralisée des raccourcis clavier (SHIFT + …) pour Brick.
 * @ingroup ui
 */

#include <stdbool.h>
#include <stdint.h>
#include "ui_input.h"   /* ui_input_event_t */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialise l’état interne des raccourcis (SM MUTE, timers, etc.). */
void ui_shortcuts_init(void);

/** Réinitialise la machine d’état (équivalent init partiel). */
void ui_shortcuts_reset(void);

/**
 * @brief Traite un évènement d’entrée. Retourne `true` si consommé.
 * @param evt Évènement d’entrée neutre.
 * @return `true` si l’évènement a été consommé par un raccourci.
 */
bool ui_shortcuts_handle_event(const ui_input_event_t *evt);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_SHORTCUTS_H */
