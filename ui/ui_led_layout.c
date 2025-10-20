/**
 * @file ui_led_layout.c
 * @brief Shared LED layout tables stored in flash (.rodata).
 */

#include "ui_led_layout.h"
#include "drv_leds_addr.h"

const uint8_t k_ui_led_seq_step_to_index[UI_LED_SEQ_STEP_COUNT] = {
    LED_SEQ1,  LED_SEQ2,  LED_SEQ3,  LED_SEQ4,
    LED_SEQ5,  LED_SEQ6,  LED_SEQ7,  LED_SEQ8,
    LED_SEQ9,  LED_SEQ10, LED_SEQ11, LED_SEQ12,
    LED_SEQ13, LED_SEQ14, LED_SEQ15, LED_SEQ16,
};
