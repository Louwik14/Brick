/**
 * @file ui_knob.h
 * @brief API de rendu du knob (plein/arc) pour Brick UI.
 */

#ifndef BRICK_UI_UI_KNOB_H
#define BRICK_UI_UI_KNOB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dimensions de l’OLED (fournies par drv_display.h dans le .c) */
#ifndef OLED_WIDTH
#define OLED_WIDTH  128
#endif
#ifndef OLED_HEIGHT
#define OLED_HEIGHT 64
#endif

/**
 * @brief Fixe un diamètre explicite (px) pour tous les knobs. 0 = désactiver.
 *        Si défini, l’argument `r` passé à ui_draw_knob() est ignoré.
 */
void ui_knob_set_diameter_px(int d_px);

/**
 * @brief Dessine un knob plein (unipolaire/bipolaire) centré en (cx,cy).
 *
 * @param cx,cy Centre du knob.
 * @param r     Rayon “extérieur” du knob (peut être écrasé si un diamètre override est actif).
 * @param val   Valeur courante.
 * @param vmin  Borne min de la plage.
 * @param vmax  Borne max de la plage.
 */
void ui_draw_knob(int cx, int cy, int r, int val, int vmin, int vmax);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_UI_UI_KNOB_H */
