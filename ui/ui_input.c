/**
 * @file ui_input.c
 * @brief Implémentation du polling combiné (boutons + encodeurs).
 *
 * @ingroup ui
 *
 * Fournit la fonction `ui_input_poll()` qui regroupe :
 * - la lecture des boutons physiques via `drv_buttons_poll()`
 * - la lecture incrémentale des encodeurs via `drv_encoder_get_delta_accel()`.
 *
 * Note: Les types drivers (button_event_t, encoder_id_t, etc.) sont confinés
 *       à ce fichier .c pour préserver un header UI neutre.
 */

#include "ui_input.h"

/* Dépendances drivers confinées à l'implémentation */
#include "drv_buttons.h"
#include "drv_encoders.h"
#include "drv_buttons_map.h"  /* BTN_* et drv_btn_pressed() */

/* ---------------------------------------------------------------------- */
/*                   Traduction des IDs drivers → IDs UI                  */
/* ---------------------------------------------------------------------- */

/**
 * @brief Traduit un identifiant de bouton driver en identifiant UI neutre.
 * @param drv_id Identifiant provenant de `drv_buttons` / `drv_buttons_map.h`
 * @return Valeur de @ref ui_button_id_t (ou UI_BTN_UNKNOWN si non mappé)
 */
static uint8_t map_driver_btn_to_ui(uint8_t drv_id) {
    switch (drv_id) {
    case BTN_PARAM1: return UI_BTN_PARAM1;
            case BTN_PARAM2: return UI_BTN_PARAM2;
            case BTN_PARAM3: return UI_BTN_PARAM3;
            case BTN_PARAM4: return UI_BTN_PARAM4;
            case BTN_PARAM5: return UI_BTN_PARAM5;
            case BTN_PARAM6: return UI_BTN_PARAM6;
            case BTN_PARAM7: return UI_BTN_PARAM7;
            case BTN_PARAM8: return UI_BTN_PARAM8;

            /* Pages */
            case BTN_PAGE1:  return UI_BTN_PAGE1;
            case BTN_PAGE2:  return UI_BTN_PAGE2;
            case BTN_PAGE3:  return UI_BTN_PAGE3;
            case BTN_PAGE4:  return UI_BTN_PAGE4;
            case BTN_PAGE5:  return UI_BTN_PAGE5;

            /* Séquenceur (SEQ1..SEQ16) */
            case BTN_SEQ1:   return UI_BTN_SEQ1;
            case BTN_SEQ2:   return UI_BTN_SEQ2;
            case BTN_SEQ3:   return UI_BTN_SEQ3;
            case BTN_SEQ4:   return UI_BTN_SEQ4;
            case BTN_SEQ5:   return UI_BTN_SEQ5;
            case BTN_SEQ6:   return UI_BTN_SEQ6;
            case BTN_SEQ7:   return UI_BTN_SEQ7;
            case BTN_SEQ8:   return UI_BTN_SEQ8;
            case BTN_SEQ9:   return UI_BTN_SEQ9;
            case BTN_SEQ10:  return UI_BTN_SEQ10;
            case BTN_SEQ11:  return UI_BTN_SEQ11;
            case BTN_SEQ12:  return UI_BTN_SEQ12;
            case BTN_SEQ13:  return UI_BTN_SEQ13;
            case BTN_SEQ14:  return UI_BTN_SEQ14;
            case BTN_SEQ15:  return UI_BTN_SEQ15;
            case BTN_SEQ16:  return UI_BTN_SEQ16;

            /* Transport */
            case BTN_PLAY:   return UI_BTN_PLAY;
            case BTN_STOP:   return UI_BTN_STOP;
            case BTN_REC:    return UI_BTN_REC;

            /* Spéciaux */
            case BTN_PLUS:   return UI_BTN_PLUS;
            case BTN_MINUS:  return UI_BTN_MINUS;

        default:         return UI_BTN_UNKNOWN;
    }
}

/* ---------------------------------------------------------------------- */

bool ui_input_poll(ui_input_event_t *evt, systime_t timeout) {
    /* Initialisation des champs neutres UI */
    evt->has_button   = false;
    evt->has_encoder  = false;
    evt->btn_id       = UI_BTN_UNKNOWN;
    evt->btn_pressed  = false;
    evt->encoder      = 0;
    evt->enc_delta    = 0;

    /* 1) Boutons (bloquant jusqu'à timeout) */
    button_event_t be;
    if (drv_buttons_poll(&be, timeout)) {
        evt->has_button  = true;
        evt->btn_id      = map_driver_btn_to_ui((uint8_t)be.id);
        evt->btn_pressed = (be.type == BUTTON_EVENT_PRESS);
    }

    /* 2) Encodeurs (non bloquant) : on prend le premier delta non nul */
    for (int i = 0; i < NUM_ENCODERS; i++) {
        int delta = drv_encoder_get_delta_accel((encoder_id_t)i);
        if (delta != 0) {
            evt->has_encoder = true;
            evt->encoder     = (uint8_t)i;
            evt->enc_delta   = (int16_t)delta;
            break;
        }
    }

    return evt->has_button || evt->has_encoder;
}

/**
 * @brief Indique si la touche SHIFT est pressée (abstraction UI).
 *
 * @return true si SHIFT est enfoncée, false sinon.
 */
bool ui_input_shift_is_pressed(void) {
    return drv_btn_pressed(BTN_SHIFT);
}
