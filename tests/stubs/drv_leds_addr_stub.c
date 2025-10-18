#include <string.h>
#include "drv_leds_addr.h"

led_state_t drv_leds_addr_state[NUM_ADRESS_LEDS];

void drv_leds_addr_init(void)
{
    memset(drv_leds_addr_state, 0, sizeof(drv_leds_addr_state));
}

void drv_leds_addr_update(void) {}

void drv_leds_addr_set_rgb(int index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < 0 || index >= NUM_ADRESS_LEDS) {
        return;
    }
    drv_leds_addr_state[index].color = (led_color_t){g, r, b};
    drv_leds_addr_state[index].mode = LED_MODE_ON;
}

void drv_leds_addr_set_color(int index, led_color_t color)
{
    if (index < 0 || index >= NUM_ADRESS_LEDS) {
        return;
    }
    drv_leds_addr_state[index].color = color;
}

void drv_leds_addr_clear(void)
{
    for (int i = 0; i < NUM_ADRESS_LEDS; ++i) {
        drv_leds_addr_state[i].color = (led_color_t){0, 0, 0};
        drv_leds_addr_state[i].mode = LED_MODE_OFF;
    }
}

void drv_leds_addr_set(int index, led_color_t color, led_mode_t mode)
{
    if (index < 0 || index >= NUM_ADRESS_LEDS) {
        return;
    }
    drv_leds_addr_state[index].color = color;
    drv_leds_addr_state[index].mode = mode;
}

void drv_leds_addr_render(void) {}
