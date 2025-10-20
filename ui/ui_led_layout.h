/**
 * @file ui_led_layout.h
 * @brief Shared LED layout tables for UI backends and renderers.
 */

#ifndef BRICK_UI_LED_LAYOUT_H
#define BRICK_UI_LED_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of sequencer LEDs mapped in the layout table. */
#define UI_LED_SEQ_STEP_COUNT 16U

/**
 * @brief Mapping table from logical sequencer step (0..15) to physical LED index.
 */
extern const uint8_t k_ui_led_seq_step_to_index[UI_LED_SEQ_STEP_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_LED_LAYOUT_H */
