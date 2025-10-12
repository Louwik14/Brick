/**
 * @file ui_led_backend.h
 * @brief Backend unifié de gestion des LEDs adressables (SK6812/WS2812) — Phase 6.
 * @ingroup ui
 */

#ifndef BRICK_UI_LED_BACKEND_H
#define BRICK_UI_LED_BACKEND_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
  UI_LED_EVENT_STEP_STATE = 0,
  UI_LED_EVENT_MUTE_STATE,
  UI_LED_EVENT_PMUTE_STATE,
  UI_LED_EVENT_CLOCK_TICK,
  UI_LED_EVENT_PARAM_SELECT
} ui_led_event_t;

typedef enum {
  UI_LED_MODE_NONE = 0,
  UI_LED_MODE_MUTE,
  UI_LED_MODE_SEQ,
  UI_LED_MODE_ARP,
  UI_LED_MODE_KEYBOARD,
  UI_LED_MODE_RANDOM,
  UI_LED_MODE_CUSTOM
} ui_led_mode_t;

/* ===== API ===== */
void ui_led_backend_init(void);
void ui_led_backend_process_event(ui_led_event_t event, uint8_t index, bool state);
void ui_led_backend_refresh(void);

void ui_led_backend_set_record_mode(bool active);
void ui_led_backend_set_mode(ui_led_mode_t mode);
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks);

/* Keyboard-specific */
void ui_led_backend_set_keyboard_omnichord(bool enabled);

#endif /* BRICK_UI_LED_BACKEND_H */
