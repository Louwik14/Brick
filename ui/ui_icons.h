/**
 * @file ui_icons.h
 * @brief Définitions et rendu d'icônes bitmap 20×14 pour l'UI Brick.
 * @ingroup ui
 *
 * @details
 * Format: 14 lignes de 20 bits, stockées dans un ui_icon_t (uint32_t[14]).
 * Rendu pixel-par-pixel dans le framebuffer OLED (aucune dépendance drivers externes).
 *
 * Usage typique:
 *   UIW_ICON_DEFINE(UI_ICON_SQUARE, ...14 lignes...);
 *   ui_icon_draw(&UI_ICON_SQUARE, x, y, true);
 */
#ifndef BRICK_UI_UI_ICONS_H
#define BRICK_UI_UI_ICONS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_ICON_WIDTH   20
#define UI_ICON_HEIGHT  14

typedef struct {
    const uint32_t data[UI_ICON_HEIGHT];
} ui_icon_t;

/**
 * @brief Macro compatible avec icon_converter.py
 * @param name  Identifiant C de l'icône (ex: UI_ICON_SINE)
 * @param ...   14 lignes binaires 0bXXXXXXXXXXXXXXXXXXXX (20 bits utiles)
 */
#define UIW_ICON_DEFINE(name, ...) \
    const ui_icon_t name = { { __VA_ARGS__ } }

/**
 * @brief Dessine une icône à (x,y) dans le framebuffer (pixels ON si on==true).
 */
void ui_icon_draw(const ui_icon_t* icon, int x, int y, bool on);

/* ============================================================
 * Déclarations des icônes disponibles (migrées)
 * ============================================================ */
extern const ui_icon_t UI_ICON_SINE;
extern const ui_icon_t UI_ICON_SQUARE;
extern const ui_icon_t UI_ICON_SAW;     /* Saw descendante */
extern const ui_icon_t UI_ICON_SAWD;    /* Saw montante */
extern const ui_icon_t UI_ICON_SAWU;    /* Saw descendante inverse */
extern const ui_icon_t UI_ICON_TRIANGLE;
extern const ui_icon_t UI_ICON_NOISE;
extern const ui_icon_t UI_ICON_LP;
extern const ui_icon_t UI_ICON_HP;
extern const ui_icon_t UI_ICON_BP;
extern const ui_icon_t UI_ICON_NOTCH;
extern const ui_icon_t UI_ICON_OFF;
extern const ui_icon_t UI_ICON_ON;



#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_ICONS_H */
