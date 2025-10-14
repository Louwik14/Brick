/**
 * @file ui_led_palette.h
 * @brief Palette centralisée des couleurs LED pour Brick (Phase 6, format GRB).
 * @ingroup ui
 *
 * @details
 * Valeurs en **GRB** (attendues par drv_leds_addr).
 * - C1..C4 : couleurs d’activité par cartouche (modes MUTE/SEQ…)
 * - MUTE : rouge universel
 * - REC : OFF/rouge
 * - Playhead : vert
 * - KEYBOARD : bleu froid (normal), bleu froid atténué (2e rangée),
 *              et 8 couleurs distinctes pour la zone "chords" en Omnichord.
 */

#ifndef BRICK_UI_LED_PALETTE_H
#define BRICK_UI_LED_PALETTE_H

#include "drv_leds_addr.h"

/* ===== Couleurs par cartouche (état actif) — GRB ======================= */
#define UI_LED_COL_CART1_ACTIVE   (led_color_t){0,   0,   180}
#define UI_LED_COL_CART2_ACTIVE   (led_color_t){180, 180, 0}
#define UI_LED_COL_CART3_ACTIVE   (led_color_t){0,   120, 120}
#define UI_LED_COL_CART4_ACTIVE   (led_color_t){180, 0,   120}
/* ===== SEQ palette (GRB) ================================================== */
#define UI_LED_COL_SEQ_PLAYHEAD   (led_color_t){255, 255, 255}  /* Blanc */
#define UI_LED_COL_SEQ_ACTIVE     (led_color_t){255, 0,   0}    /* Vert  */
#define UI_LED_COL_SEQ_RECORDED   (led_color_t){255, 255, 0}    /* Jaune */
#define UI_LED_COL_SEQ_PARAM      (led_color_t){0,   0,   255}  /* Bleu  */
#define UI_LED_COL_SEQ_PLOCKED    (led_color_t){0,   128, 255}  /* Violet */
#define UI_LED_COL_SEQ_AUTOMATION (led_color_t){64,  0,   255}  /* Cyan atténué */

/* ===== États globaux — GRB ============================================= */
#define UI_LED_COL_MUTE_RED       (led_color_t){0,   180, 0}
#define UI_LED_COL_OFF            (led_color_t){0,   0,   0}

/* ===== Système / indicateurs — GRB ===================================== */
#define UI_LED_COL_REC_ACTIVE     (led_color_t){0,   180, 0}
#define UI_LED_COL_PLAYHEAD       (led_color_t){255, 255,   255}

/* ===== KEYBOARD mode — GRB ============================================= */
#define UI_LED_COL_KEY_BLUE_HI    (led_color_t){0,   0,   255}  /* rangée 1 */
#define UI_LED_COL_KEY_BLUE_LO    (led_color_t){0,   0,    64}  /* rangée 2 */

/* 8 couleurs distinctes (zone chords Omnichord) — GRB */
#define UI_LED_COL_CHORD_1        (led_color_t){0,   255,   0}  /* Rouge  */
#define UI_LED_COL_CHORD_2        (led_color_t){255, 0,     0}  /* Vert   */
#define UI_LED_COL_CHORD_3        (led_color_t){0,   128, 128}  /* Violet */
#define UI_LED_COL_CHORD_4        (led_color_t){64,  255,   0}  /* Orange */
#define UI_LED_COL_CHORD_5        (led_color_t){255, 255,   0}  /* Jaune  */
#define UI_LED_COL_CHORD_6        (led_color_t){255, 0,   255}  /* Cyan   */
#define UI_LED_COL_CHORD_7        (led_color_t){0,   200, 200}  /* Rose   */
#define UI_LED_COL_CHORD_8        (led_color_t){180, 0,   120}  /* Teal   */

#endif /* BRICK_UI_LED_PALETTE_H */
