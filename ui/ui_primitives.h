/**
 * @file ui_primitives.h
 * @brief Primitives graphiques bas-niveau pour le dessin direct dans le framebuffer OLED.
 *
 * @ingroup ui
 *
 * Ce module fournit un ensemble de fonctions inline pour manipuler directement
 * le framebuffer retourné par `drv_display_get_buffer()`.
 * Il est indépendant du renderer principal (`ui_renderer.c`)
 * et peut être utilisé par les widgets (`ui_widgets`) ou d’autres modules graphiques.
 *
 * ⚙️ Toutes les fonctions sont `static inline` pour éviter les surcoûts d’appel
 * et permettre une inclusion simple dans plusieurs fichiers.
 */

#ifndef BRICK_SOMEPATH_SOMEFILE_H
#define BRICK_SOMEPATH_SOMEFILE_H

#include <drv_display.h>
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Fonctions de base : pixels et lignes
 * ============================================================ */

/**
 * @brief Active ou efface un pixel dans le framebuffer.
 *
 * @param x  Coordonnée horizontale (0..OLED_WIDTH-1)
 * @param y  Coordonnée verticale (0..OLED_HEIGHT-1)
 * @param on true = pixel allumé, false = pixel éteint
 */
static inline void ui_px(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    uint8_t *buf = drv_display_get_buffer();
    const int index = x + (y >> 3) * OLED_WIDTH;
    const uint8_t mask = (uint8_t)(1u << (y & 7));
    if (on) buf[index] |= mask;
    else    buf[index] &= (uint8_t)~mask;
}

/**
 * @brief Trace une ligne horizontale d’un seul pixel de hauteur.
 *
 * @param x  Position de départ en X
 * @param y  Position verticale
 * @param w  Largeur en pixels
 * @param on true = dessine, false = efface
 */
static inline void ui_hline(int x, int y, int w, bool on) {
    for (int i = 0; i < w; ++i)
        ui_px(x + i, y, on);
}

/**
 * @brief Trace une ligne verticale d’un seul pixel de largeur.
 *
 * @param x  Position horizontale
 * @param y  Position de départ verticale
 * @param h  Hauteur en pixels
 * @param on true = dessine, false = efface
 */
static inline void ui_vline(int x, int y, int h, bool on) {
    for (int i = 0; i < h; ++i)
        ui_px(x, y + i, on);
}

/* ============================================================
 * Formes rectangulaires
 * ============================================================ */

/**
 * @brief Trace un rectangle vide (cadre) avec des bords d’un pixel.
 *
 * @param x  Coin supérieur gauche X
 * @param y  Coin supérieur gauche Y
 * @param w  Largeur du rectangle
 * @param h  Hauteur du rectangle
 * @param on true = dessine, false = efface
 */
static inline void ui_rect(int x, int y, int w, int h, bool on) {
    if (w <= 0 || h <= 0) return;
    ui_hline(x, y, w, on);
    ui_hline(x, y + h - 1, w, on);
    ui_vline(x, y, h, on);
    ui_vline(x + w - 1, y, h, on);
}

/**
 * @brief Remplit un rectangle plein.
 *
 * @param x  Coin supérieur gauche X
 * @param y  Coin supérieur gauche Y
 * @param w  Largeur
 * @param h  Hauteur
 * @param on true = dessine, false = efface
 */
static inline void ui_fill_rect(int x, int y, int w, int h, bool on) {
    for (int yy = 0; yy < h; ++yy)
        ui_hline(x, y + yy, w, on);
}

/* ============================================================
 * Blitting (copie de bitmaps 1bpp)
 * ============================================================ */

/**
 * @brief Copie un bitmap monochrome (1 bit/pixel) vers le framebuffer.
 *
 * @param x             Position X de destination
 * @param y             Position Y de destination
 * @param w             Largeur du bitmap en pixels
 * @param h             Hauteur du bitmap en pixels
 * @param bits          Pointeur vers les données source (1 bit/pixel)
 * @param stride_bytes  Nombre d’octets par rangée (>= (w + 7) / 8)
 *
 * Chaque rangée du bitmap est lue MSB → LSB (bit 7 = pixel le plus à gauche du byte).
 */
static inline void ui_blit_mono(int x, int y, int w, int h,
                                const uint8_t *bits, int stride_bytes) {
    for (int yy = 0; yy < h; ++yy) {
        const uint8_t *row = bits + yy * stride_bytes;
        int xx = 0;
        for (int b = 0; b < stride_bytes && xx < w; ++b) {
            uint8_t v = row[b];
            for (int bit = 7; bit >= 0 && xx < w; --bit, ++xx) {
                bool on = ((v >> bit) & 1u) != 0;
                ui_px(x + xx, y + yy, on);
            }
        }
    }
}
