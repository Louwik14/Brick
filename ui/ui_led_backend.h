/**
 * @file ui_led_backend.h
 * @brief Backend unifié de gestion des LEDs adressables (SK6812/WS2812) — Phase 6 (+ SEQ renderer).
 * @ingroup ui
 *
 * @details
 * - Fait office de routeur de rendu selon le mode actif (MUTE / KEYBOARD / SEQ / …).
 * - Reçoit des événements (MUTE/PMUTE, TICK, …) via `ui_led_backend_process_event`.
 * - **NOUVEAU** : en mode `UI_LED_MODE_SEQ`, délègue le rendu à `ui_led_seq_render()`
 *   et relaie les ticks clock vers `ui_led_seq_on_clock_tick()`.
 */

#ifndef BRICK_UI_LED_BACKEND_H
#define BRICK_UI_LED_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

/** @ingroup ui */
typedef enum {
  UI_LED_EVENT_STEP_STATE = 0,
  UI_LED_EVENT_MUTE_STATE,
  UI_LED_EVENT_PMUTE_STATE,
  UI_LED_EVENT_CLOCK_TICK,
  UI_LED_EVENT_PARAM_SELECT
} ui_led_event_t;

/** @ingroup ui */
typedef enum {
  UI_LED_MODE_NONE = 0,
  UI_LED_MODE_MUTE,
  UI_LED_MODE_SEQ,
  UI_LED_MODE_ARP,
  UI_LED_MODE_KEYBOARD,
  UI_LED_MODE_RANDOM,
  UI_LED_MODE_CUSTOM
} ui_led_mode_t;

/* ===== API principale ===== */
/** @brief Init du backend (driver + état visuel). */
void ui_led_backend_init(void);

/**
 * @brief Injection d’évènement LED.
 * @param event Type d’événement (MUTE/PMUTE/CLK/…)
 * @param index Index associé (ex: step 0..15 pour CLOCK_TICK)
 * @param state Booléen associé si pertinent
 */
void ui_led_backend_process_event(ui_led_event_t event, uint8_t index, bool state);

/** @brief Rendu par mode (à appeler périodiquement avant drv_leds_addr_render()). */
void ui_led_backend_refresh(void);

/** @brief LED REC globale (OFF/ON). */
void ui_led_backend_set_record_mode(bool active);

/** @brief Sélection du mode visuel courant. */
void ui_led_backend_set_mode(ui_led_mode_t mode);

/** @brief Pour MUTE : nombre de pistes par cart (0..4). */
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks);

/* ===== Spécifique Keyboard (inchangé) ===== */
/** @brief Active/valide le layout Omnichord (Keyboard). */
void ui_led_backend_set_keyboard_omnichord(bool enabled);

#endif /* BRICK_UI_LED_BACKEND_H */
