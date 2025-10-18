/**
 * @file drv_display.c
 * @brief Driver OLED SPI **SSD1309** (128×64) pour Brick.
 *
 * Ce module implémente le pilotage complet d’un écran OLED monochrome
 * via l’interface **SPI1** du STM32.
 *
 * Fonctions principales :
 * - Initialisation matérielle du contrôleur SSD1309.
 * - Gestion d’un framebuffer local en RAM.
 * - Routines de dessin (texte, pixels, caractères).
 * - Thread de rafraîchissement périodique (~30 FPS).
 *
 * @note Communication SPI :
 *   - **CS** : PB4
 *   - **D/C** : PB5
 *   - **MOSI/SCK** configurés par `halconf.h`
 *
 * @ingroup drivers
 */

#include "drv_display.h"
#include "ch.h"
#include "hal.h"
#include "brick_config.h"
#include <string.h>

/* ====================================================================== */
/*                        CONFIGURATION MATÉRIELLE                        */
/* ====================================================================== */

/** @brief Configuration du périphérique SPI (SPI1). */
static const SPIConfig spicfg = {
    .ssport = GPIOB,
    .sspad  = 4,            /**< CS sur PB4 */
    .cr1    = SPI_CR1_BR_2, /**< Horloge SPI = PCLK / 8 */
    .cr2    = 0
};

/* ====================================================================== */
/*                             VARIABLES INTERNES                         */
/* ====================================================================== */

/** @brief Framebuffer local (1 bit/pixel). */
static uint8_t buffer[OLED_WIDTH * OLED_HEIGHT / 8];

/** @brief Police de caractères actuellement utilisée. */
static const font_t *current_font = NULL;

/* ====================================================================== */
/*                              UTILITAIRES SPI                           */
/* ====================================================================== */

static inline void dc_cmd(void)  { palClearPad(GPIOB, 5); } /**< Ligne D/C = 0 (commande). */
static inline void dc_data(void) { palSetPad(GPIOB, 5);   } /**< Ligne D/C = 1 (donnée). */

/**
 * @brief Envoie une commande 8 bits au contrôleur SSD1309.
 */
static void send_cmd(uint8_t cmd) {
    dc_cmd();
    spiSelect(&SPID1);
    spiSend(&SPID1, 1, &cmd);
    spiUnselect(&SPID1);
}

/**
 * @brief Envoie un bloc de données au contrôleur SSD1309.
 * @param data Pointeur sur le buffer de données.
 * @param len  Nombre d’octets à transmettre.
 */
static void send_data(const uint8_t *data, size_t len) {
    dc_data();
    spiSelect(&SPID1);
    spiSend(&SPID1, len, data);
    spiUnselect(&SPID1);
}

/* ====================================================================== */
/*                              FRAMEBUFFER                               */
/* ====================================================================== */

/**
 * @brief Retourne un pointeur sur le framebuffer interne.
 */
uint8_t* drv_display_get_buffer(void) { return buffer; }

/* ====================================================================== */
/*                              PIXELS / DESSIN                           */
/* ====================================================================== */

/**
 * @brief Allume ou éteint un pixel dans le framebuffer.
 *
 * @param x Coordonnée horizontale (0–127)
 * @param y Coordonnée verticale (0–63)
 * @param on `true` pour allumer le pixel, `false` pour l’éteindre.
 */
static inline void set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    const int index = x + (y >> 3) * OLED_WIDTH;     // y/8 = page
    const uint8_t mask = (uint8_t)(1U << (y & 7));   // bit dans la page
    if (on) buffer[index] |=  mask;
    else    buffer[index] &= (uint8_t)~mask;
}

/* ====================================================================== */
/*                      INITIALISATION ET MISE À JOUR                     */
/* ====================================================================== */

/**
 * @brief Initialise l’écran OLED et configure le SSD1309.
 */
void drv_display_init(void) {
    spiStart(&SPID1, &spicfg);

    /* Séquence d’initialisation SSD1309 */
    send_cmd(0xAE);
    send_cmd(0xD5); send_cmd(0x80);
    send_cmd(0xA8); send_cmd(0x3F);
    send_cmd(0xD3); send_cmd(0x00);
    send_cmd(0x40);
    send_cmd(0x8D); send_cmd(0x14);
    send_cmd(0x20); send_cmd(0x02); // Page addressing mode
    send_cmd(0xA1);                 // Miroir horizontal
    send_cmd(0xC8);                 // Miroir vertical
    send_cmd(0xDA); send_cmd(0x12);
    send_cmd(0x81); send_cmd(0x7F);
    send_cmd(0xD9); send_cmd(0xF1);
    send_cmd(0xDB); send_cmd(0x40);
    send_cmd(0xA4);
    send_cmd(0xA6);
    send_cmd(0x21); send_cmd(0x00); send_cmd(0x7F);
    send_cmd(0xAF);

    drv_display_clear();

    extern const font_t FONT_5X7;
    current_font = &FONT_5X7;
}

/**
 * @brief Efface complètement le framebuffer.
 */
void drv_display_clear(void) {
    memset(buffer, 0x00, sizeof(buffer));
}

/**
 * @brief Transfère le contenu du framebuffer vers l’écran OLED.
 */
void drv_display_update(void) {
    for (uint8_t page = 0; page < 8; page++) {
        send_cmd(0xB0 + page);
        send_cmd(0x00);
        send_cmd(0x10);
        send_cmd(0x21); send_cmd(0x00); send_cmd(0x7F);
        send_data(&buffer[page * OLED_WIDTH], OLED_WIDTH);
    }
}

/* ====================================================================== */
/*                           GESTION DES POLICES                          */
/* ====================================================================== */

/**
 * @brief Définit la police courante utilisée pour le rendu texte.
 */
void drv_display_set_font(const font_t *font) {
    current_font = font;
}

/* ====================================================================== */
/*                             DESSIN DU TEXTE                            */
/* ====================================================================== */

/**
 * @brief Dessine un caractère à une position donnée.
 */
void drv_display_draw_char(uint8_t x, uint8_t y, char c) {
    if (!current_font) return;
    if ((uint8_t)c < current_font->first || (uint8_t)c > current_font->last) c = '?';

    for (uint8_t col = 0; col < current_font->width; col++) {
        uint8_t bits = current_font->get_col(c, col);
        for (uint8_t row = 0; row < current_font->height; row++) {
            if (bits & (1U << row)) set_pixel(x + col, y + row, true);
        }
    }
}

/** @brief Calcule l’avance horizontale d’une police. */
static inline uint8_t font_advance(const font_t *f) {
    return (uint8_t)(f->width + f->spacing);
}

/**
 * @brief Dessine une chaîne de caractères à partir d’une position.
 */
void drv_display_draw_text(uint8_t x, uint8_t y, const char *txt) {
    if (!current_font || !txt) return;
    const uint8_t adv = font_advance(current_font);
    while (*txt && x < OLED_WIDTH) {
        drv_display_draw_char(x, y, *txt++);
        x = (uint8_t)(x + adv);
    }
}

/**
 * @brief Dessine une chaîne en utilisant une police spécifique.
 */
void drv_display_draw_text_with_font(const font_t *font, uint8_t x, uint8_t y, const char *txt) {
    if (!font || !txt) return;
    const font_t *save = current_font;
    current_font = font;
    drv_display_draw_text(x, y, txt);
    current_font = save;
}

/**
 * @brief Dessine du texte aligné sur une ligne de base.
 */
void drv_display_draw_text_at_baseline(const font_t *font, uint8_t x, uint8_t baseline_y, const char *txt) {
    if (!font || !txt) return;
    const font_t *save = current_font;
    current_font = font;
    uint8_t y = (baseline_y >= font->height) ? (baseline_y - font->height) : 0;
    drv_display_draw_text(x, y, txt);
    current_font = save;
}

/**
 * @brief Affiche un entier décimal à la position donnée.
 */
void drv_display_draw_number(uint8_t x, uint8_t y, int num) {
    char buf[16];
    chsnprintf(buf, sizeof(buf), "%d   ", num);
    drv_display_draw_text(x, y, buf);
}

/**
 * @brief Dessine un caractère centré dans une boîte rectangulaire.
 */
void drv_display_draw_char_in_box(const font_t *font, uint8_t x, uint8_t y, uint8_t box_w, uint8_t box_h, char c) {
    if (!font) return;
    uint8_t off_x = 0, off_y = 0;
    if (box_w > font->width)  off_x = (uint8_t)((box_w - font->width) / 2);
    if (box_h > font->height) off_y = (uint8_t)((box_h - font->height) / 2);

    const font_t *save = current_font;
    current_font = font;
    drv_display_draw_char((uint8_t)(x + off_x), (uint8_t)(y + off_y), c);
    current_font = save;
}

/* ====================================================================== */
/*                         THREAD DE RAFRAÎCHISSEMENT                     */
/* ====================================================================== */

/**
 * @brief Thread d’affichage : met à jour l’écran à ~30 FPS.
 */
static CCM_DATA THD_WORKING_AREA(waDisplay, 2048);
static THD_FUNCTION(displayThread, arg) {
    (void)arg;
    chRegSetThreadName("Display");
    while (true) {
        drv_display_update();
        chThdSleepMilliseconds(33);
    }
}

/**
 * @brief Démarre le thread d’affichage automatique.
 */
void drv_display_start(void) {
    drv_display_init();
    chThdCreateStatic(waDisplay, sizeof(waDisplay), NORMALPRIO, displayThread, NULL);
}
