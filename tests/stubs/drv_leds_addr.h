#ifndef TESTS_STUBS_DRV_LEDS_ADDR_H
#define TESTS_STUBS_DRV_LEDS_ADDR_H

#include <stdint.h>
#include "core/brick_config.h"

typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} led_color_t;

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_ON,
    LED_MODE_BLINK,
    LED_MODE_PLAYHEAD
} led_mode_t;

typedef struct {
    led_color_t color;
    led_mode_t mode;
} led_state_t;

#define LED_REC   0
#define LED_SEQ8  1
#define LED_SEQ7  2
#define LED_SEQ6  3
#define LED_SEQ5  4
#define LED_SEQ4  5
#define LED_SEQ3  6
#define LED_SEQ2  7
#define LED_SEQ1  8
#define LED_SEQ9  9
#define LED_SEQ10 10
#define LED_SEQ11 11
#define LED_SEQ12 12
#define LED_SEQ13 13
#define LED_SEQ14 14
#define LED_SEQ15 15
#define LED_SEQ16 16

extern led_state_t drv_leds_addr_state[NUM_ADRESS_LEDS];

void drv_leds_addr_init(void);
void drv_leds_addr_update(void);
void drv_leds_addr_set_rgb(int index, uint8_t r, uint8_t g, uint8_t b);
void drv_leds_addr_set_color(int index, led_color_t color);
void drv_leds_addr_clear(void);
void drv_leds_addr_set(int index, led_color_t color, led_mode_t mode);
void drv_leds_addr_render(void);

#endif /* TESTS_STUBS_DRV_LEDS_ADDR_H */
