/**
 * @file seq_led_bridge.h
 * @brief Thread-safe LED bridge between sequencer runtime and UI renderer.
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */

#ifndef BRICK_SEQ_LED_BRIDGE_H
#define BRICK_SEQ_LED_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_led_seq.h"

#ifdef __cplusplus
extern "C" {
#endif

void seq_led_bridge_init(void);
void seq_led_bridge_publish(void);
void seq_led_bridge_set_max_pages(uint8_t max_pages);
void seq_led_bridge_set_total_span(uint16_t total_steps);
void seq_led_bridge_page_next(void);
void seq_led_bridge_page_prev(void);
void seq_led_bridge_set_visible_page(uint8_t page);
void seq_led_bridge_quick_toggle_step(uint8_t index);
void seq_led_bridge_set_step_param_only(uint8_t index, bool on);
void seq_led_bridge_on_play(void);
void seq_led_bridge_on_stop(void);
void seq_led_bridge_set_plock_mask(uint16_t mask);
void seq_led_bridge_plock_add(uint8_t index);
void seq_led_bridge_plock_remove(uint8_t index);
void seq_led_bridge_plock_clear(void);
void seq_led_bridge_begin_plock_preview(uint16_t held_mask);
void seq_led_bridge_apply_plock_param(uint8_t param_id, int32_t delta, uint16_t held_mask);
void seq_led_bridge_end_plock_preview(void);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_LED_BRIDGE_H */
