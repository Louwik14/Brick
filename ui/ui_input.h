#ifndef BRICK_UI_UI_INPUT_H
#define BRICK_UI_UI_INPUT_H

/**
 * @file ui_input.h
 * @brief Abstraction des entrées utilisateur (événements UI neutres).
 * @ingroup ui
 *
 * Ce header ne dépend d'aucun driver. Le mapping Drivers → UI est fait
 * exclusivement dans ui_input.c.
 */

#include "ch.h"       /* pour systime_t ; remplaçable par uint32_t si souhaité */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Identifiants de boutons **neutres UI** (indépendants des drivers).
 *
 * Ces valeurs sont produites par @ref ui_input_poll après traduction depuis
 * les IDs matériels (drv_buttons_map.h), réalisée **dans ui_input.c**.
 */
typedef enum {
    UI_BTN_PARAM1 = 0,  /**< Bouton Menu 1  */
    UI_BTN_PARAM2,      /**< Bouton Menu 2  */
    UI_BTN_PARAM3,      /**< Bouton Menu 3  */
    UI_BTN_PARAM4,      /**< Bouton Menu 4  */
    UI_BTN_PARAM5,      /**< Bouton Menu 5  */
    UI_BTN_PARAM6,      /**< Bouton Menu 6  */
    UI_BTN_PARAM7,      /**< Bouton Menu 7  */
    UI_BTN_PARAM8,      /**< Bouton Menu 8  */
    UI_BTN_PAGE1,       /**< Bouton Page 1  */
    UI_BTN_PAGE2,       /**< Bouton Page 2  */
    UI_BTN_PAGE3,       /**< Bouton Page 3  */
    UI_BTN_PAGE4,       /**< Bouton Page 4  */
    UI_BTN_PAGE5,       /**< Bouton Page 5  */
    UI_BTN_PLAY,
    UI_BTN_STOP,
    UI_BTN_REC,
    UI_BTN_PLUS,
    UI_BTN_MINUS,
    UI_BTN_SEQ1,
    UI_BTN_SEQ2,
    UI_BTN_SEQ3,
    UI_BTN_SEQ4,
    UI_BTN_SEQ5,
    UI_BTN_SEQ6,
    UI_BTN_SEQ7,
    UI_BTN_SEQ8,
    UI_BTN_SEQ9,
    UI_BTN_SEQ10,
    UI_BTN_SEQ11,
    UI_BTN_SEQ12,
    UI_BTN_SEQ13,
    UI_BTN_SEQ14,
    UI_BTN_SEQ15,
    UI_BTN_SEQ16,


    UI_BTN_UNKNOWN = 255
} ui_button_id_t;

/**
 * @brief Événement d'entrée UI neutre (indépendant des drivers).
 */
typedef struct {
    /* Bouton */
    uint8_t  btn_id;        /**< Identifiant bouton (voir @ref ui_button_id_t) */
    bool     btn_pressed;   /**< true=press, false=release */
    bool     has_button;    /**< Un évènement bouton est présent */

    /* Encodeur */
    uint8_t  encoder;       /**< Index encodeur (0..N-1) */
    int16_t  enc_delta;     /**< Delta signé */
    bool     has_encoder;   /**< Un évènement encodeur est présent */
} ui_input_event_t;

/**
 * @brief Poll unifié des entrées (bouton + encodeur).
 * @param evt     [out] Événement rempli si dispo
 * @param timeout Délai max d'attente (ticks ChibiOS)
 * @return true si au moins un évènement est présent
 */
bool ui_input_poll(ui_input_event_t *evt, systime_t timeout);

/**
 * @brief État de la touche SHIFT (abstraction UI).
 * @return true si SHIFT est pressée
 */
bool ui_input_shift_is_pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_INPUT_H */
