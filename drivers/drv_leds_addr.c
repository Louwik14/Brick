/**
 * @file drv_leds_addr.c
 * @brief Driver pour LEDs adressables (type WS2812B) — sortie sur port GPIOD.3.
 *
 * Implémente une génération logicielle du protocole WS2812B à 800 kHz
 * via instructions ARM inline ASM calibrées pour une fréquence CPU de **168 MHz**.
 *
 * Fonctionnalités :
 * - Transmission sérielle des données RGB (GRB order)
 * - Gestion d’un buffer mémoire `led_buffer[]`
 * - Modes d’affichage logiques : ON / OFF / BLINK / PLAYHEAD
 * - Contrôle de la luminosité globale (`LED_BRIGHTNESS`)
 *
 * @note Nécessite interruptions désactivées pendant l’envoi (chSysLock).
 *
 * @ingroup drivers
 */

#include "drv_leds_addr.h"
#include "core/ram_audit.h"

#define LED_PORT GPIOD
#define LED_PIN  3

static led_color_t led_buffer[NUM_ADRESS_LEDS];
UI_RAM_AUDIT(led_buffer);

/* === Transmission bit à bit (ASM calibré 168 MHz) === */
static inline void send_bit_asm(uint32_t mask_set, uint32_t mask_reset, int bit) {
    if (bit) {
        __asm__ volatile (
            "str %[set], [%[bsrr], #0]   \n\t" // HIGH
            ".rept 110                    \n\t"
            "nop                          \n\t"
            ".endr                        \n\t"
            "str %[reset], [%[bsrr], #0] \n\t" // LOW
            ".rept 100                    \n\t"
            "nop                          \n\t"
            ".endr                        \n\t"
            :
            : [set] "r" (mask_set),
              [reset] "r" (mask_reset),
              [bsrr] "r" (&LED_PORT->BSRR)
        );
    } else {
        __asm__ volatile (
            "str %[set], [%[bsrr], #0]   \n\t" // HIGH
            ".rept 50                     \n\t"
            "nop                          \n\t"
            ".endr                        \n\t"
            "str %[reset], [%[bsrr], #0] \n\t" // LOW
            ".rept 150                    \n\t"
            "nop                          \n\t"
            ".endr                        \n\t"
            :
            : [set] "r" (mask_set),
              [reset] "r" (mask_reset),
              [bsrr] "r" (&LED_PORT->BSRR)
        );
    }
}

/* Envoi d’un octet complet GRB */
static void send_byte_asm(uint8_t b) {
    uint32_t mask_set = (1U << LED_PIN);
    uint32_t mask_reset = (1U << (LED_PIN + 16));
    for (int i = 7; i >= 0; i--) {
        send_bit_asm(mask_set, mask_reset, (b >> i) & 1);
    }
}

/* =======================================================================
 *                              API matérielle
 * ======================================================================= */

void drv_leds_addr_init(void) {
    palSetPadMode(LED_PORT, LED_PIN, PAL_MODE_OUTPUT_PUSHPULL);
    drv_leds_addr_clear();
    drv_leds_addr_update();
}

void drv_leds_addr_update(void) {
    chSysLock();

    for (int i = 0; i < NUM_ADRESS_LEDS; i++) {
        send_byte_asm(led_buffer[i].g);
        send_byte_asm(led_buffer[i].r);
        send_byte_asm(led_buffer[i].b);
    }

    chSysUnlock();
    chThdSleepMicroseconds(300); // Reset >200µs requis
}

void drv_leds_addr_set_rgb(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= NUM_ADRESS_LEDS) return;
    led_buffer[index].r = (r * LED_BRIGHTNESS) / 255;
    led_buffer[index].g = (g * LED_BRIGHTNESS) / 255;
    led_buffer[index].b = (b * LED_BRIGHTNESS) / 255;
}

void drv_leds_addr_clear(void) {
    for (int i = 0; i < NUM_ADRESS_LEDS; i++) {
        led_buffer[i].r = 0;
        led_buffer[i].g = 0;
        led_buffer[i].b = 0;
    }
}

/* =======================================================================
 *                              API couleur
 * ======================================================================= */

void drv_leds_addr_set_color(int index, led_color_t color) {
    if (index < 0 || index >= NUM_ADRESS_LEDS) return;
    led_buffer[index].r = (color.r * LED_BRIGHTNESS) / 255;
    led_buffer[index].g = (color.g * LED_BRIGHTNESS) / 255;
    led_buffer[index].b = (color.b * LED_BRIGHTNESS) / 255;
}

/* =======================================================================
 *                              API logique
 * ======================================================================= */

led_state_t drv_leds_addr_state[NUM_ADRESS_LEDS];
UI_RAM_AUDIT(drv_leds_addr_state);

void drv_leds_addr_set(int index, led_color_t color, led_mode_t mode) {
    if (index < 0 || index >= NUM_ADRESS_LEDS) return;
    drv_leds_addr_state[index].color = color;
    drv_leds_addr_state[index].mode  = mode;
}

/**
 * @brief Rendu de l’état logique vers le buffer physique.
 *
 * Gère les effets visuels simples :
 * - `LED_MODE_ON`      → Couleur constante
 * - `LED_MODE_OFF`     → LED éteinte
 * - `LED_MODE_BLINK`   → Clignotement 2 Hz
 * - `LED_MODE_PLAYHEAD`→ Effet pulsé
 */
void drv_leds_addr_render(void) {
    static uint32_t tick = 0;
    tick++;

    drv_leds_addr_clear();

    for (int i = 0; i < NUM_ADRESS_LEDS; i++) {
        switch (drv_leds_addr_state[i].mode) {
            case LED_MODE_OFF:
                break;

            case LED_MODE_ON:
                drv_leds_addr_set_color(i, drv_leds_addr_state[i].color);
                break;

            case LED_MODE_BLINK:
                if ((tick / 20) % 2 == 0) { // ≈ 2 Hz
                    drv_leds_addr_set_color(i, drv_leds_addr_state[i].color);
                }
                break;

            case LED_MODE_PLAYHEAD:
                if ((tick % 40) < 30) { // effet “pulse”
                    drv_leds_addr_set_color(i, drv_leds_addr_state[i].color);
                }
                break;
        }
    }

    drv_leds_addr_update();
}
