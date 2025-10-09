/**
 * @file ui_widgets.c
 * @brief Widgets UI Brick — sélection de widget + rendu (icônes 20×14, switch, knob).
 *
 * @ingroup ui
 *
 * @details
 * Ce module implémente :
 *  - la sélection du type de widget à partir du kind/labels
 *    (`uiw_pick_from_labels`, `uiw_pick_from_kind_label_only`),
 *  - les routines de dessin utilisées par le renderer :
 *    `uiw_draw_switch`, `uiw_draw_knob`, `uiw_draw_knob_ex`,
 *  - un utilitaire d’icône par label texte (`uiw_draw_icon_by_text`).
 *
 * Principes :
 *  - Aucune dépendance vers ui_controller/cart_* (rendu uniquement).
 *  - Accès framebuffer via drv_display_* (icônes 20×14 rendues via ui_icon_draw()).
 *
 * Hiérarchie :
 *   ui_renderer → ui_widgets → ui_icons → drv_display
 */

/* --- Réglage optionnel : diamètre exact des knobs (px). Sinon auto-fit. --- */
// #define UIW_KNOB_DIAMETER_PX 18

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

#include "ui_widgets.h"
#include "ui_icons.h"
#include "drv_display.h"
#include "ui_knob.h"   /* ui_draw_knob(cx,cy,r,val,min,max) */

/* ==========================================================================
 *                       CONFIG KNOB (facile à ajuster)
 * ==========================================================================
 */
const uiw_knob_style_t UIW_KNOB_STYLE_DEFAULT = {
    .padding        = 0,   /* ↑ = knob plus petit dans le cadre */
    .ring_thickness = 0,   /* non utilisé par ui_knob.c */
    .fill_steps     = 0    /* non utilisé par ui_knob.c */
};

/* ==========================================================================
 *                   ICÔNES : RECONNAISSANCE PAR TEXTE CANONIQUE
 * ==========================================================================
 */

/* Normalisation simple : minuscules + suppression espaces/underscore/tirets. */
static void normalize_label(char *dst, size_t dstsz, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < dstsz; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == ' ' || c == '_' || c == '-') continue;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        dst[j++] = (char)c;
    }
    dst[j] = '\0';
}

static bool key_has(const char *key, const char *needle) {
    return key && needle && strstr(key, needle) != NULL;
}

static bool key_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

/* Mapping déterministe par tokens (indépendant de l’ordre des enums). */
static const ui_icon_t* match_icon_for_text(const char *text) {
    if (!text) return NULL;
    char key[64]; normalize_label(key, sizeof key, text);

    /* BOOL ON/OFF — égalité stricte pour éviter collisions */
    if (key_eq(key, "on")   || key_eq(key, "true"))  return &UI_ICON_ON;
    if (key_eq(key, "off")  || key_eq(key, "false")) return &UI_ICON_OFF;

    /* WAVES */
    if (key_has(key, "sine"))                           return &UI_ICON_SINE;
    if (key_has(key, "square") || key_has(key, "sqr"))  return &UI_ICON_SQUARE;
    if (key_has(key, "triangle") || key_has(key, "tri"))return &UI_ICON_TRIANGLE;
    if (key_has(key, "sawu") || key_has(key, "sawup"))  return &UI_ICON_SAWU;
    if (key_has(key, "sawd") || key_has(key, "sawdn"))  return &UI_ICON_SAWD;
    if (key_has(key, "saw"))                            return &UI_ICON_SAW;   /* générique */
    if (key_has(key, "noise"))                          return &UI_ICON_NOISE;

    /* FILTERS */
    if (key_has(key, "lowpass") || key_has(key, "lp"))  return &UI_ICON_LP;
    if (key_has(key, "highpass")|| key_has(key, "hp"))  return &UI_ICON_HP;
    if (key_has(key, "bandpass")|| key_has(key, "bp"))  return &UI_ICON_BP;
    if (key_has(key, "bandstop")|| key_has(key, "notch")|| key_has(key, "br"))
        return &UI_ICON_NOTCH;

    return NULL;
}

/**
 * @brief Dessine une icône (20×14) centrée, choisie via label texte canonique.
 * @retval true  icône dessinée, false sinon (aucun dessin).
 */
bool uiw_draw_icon_by_text(const char *text, int x, int y, int w, int h) {
    const ui_icon_t *ic = match_icon_for_text(text);
    if (!ic) return false;
    const int iw = UI_ICON_WIDTH, ih = UI_ICON_HEIGHT;
    const int x0 = x + (w - iw) / 2;
    const int y0 = y + (h - ih) / 2;
    ui_icon_draw(ic, x0, y0, true);
    return true;
}

/* ==========================================================================
 *                         SÉLECTION DU TYPE DE WIDGET
 * ==========================================================================
 */

static bool labels_contain(const char *const *labels, int n, const char *needle_norm) {
    if (!labels || n <= 0 || !needle_norm) return false;
    char key[64];
    for (int i = 0; i < n; ++i) {
        if (!labels[i]) continue;
        normalize_label(key, sizeof key, labels[i]);
        if (strstr(key, needle_norm) != NULL) return true;
    }
    return false;
}

ui_widget_type_t uiw_pick_from_labels(ui_param_kind_t kind,
                                      const char *label,
                                      const char * const *labels,
                                      int nlabels) {
    (void)label;

    if (kind == UI_PARAM_ENUM) {
        /* WAVES ? */
        if (labels_contain(labels, nlabels, "sine")   ||
            labels_contain(labels, nlabels, "square") ||
            labels_contain(labels, nlabels, "sqr")    ||
            labels_contain(labels, nlabels, "triangle") ||
            labels_contain(labels, nlabels, "tri")    ||
            labels_contain(labels, nlabels, "sawu")   ||
            labels_contain(labels, nlabels, "sawup")  ||
            labels_contain(labels, nlabels, "sawd")   ||
            labels_contain(labels, nlabels, "sawdn")  ||
            labels_contain(labels, nlabels, "saw")    ||
            labels_contain(labels, nlabels, "noise")) {
            return UIW_ENUM_ICON_WAVE;
        }

        /* FILTERS ? */
        if (labels_contain(labels, nlabels, "lowpass") ||
            labels_contain(labels, nlabels, "lp")      ||
            labels_contain(labels, nlabels, "highpass")||
            labels_contain(labels, nlabels, "hp")      ||
            labels_contain(labels, nlabels, "bandpass")||
            labels_contain(labels, nlabels, "bp")      ||
            labels_contain(labels, nlabels, "bandstop")||
            labels_contain(labels, nlabels, "notch")   ||
            labels_contain(labels, nlabels, "br")) {
            return UIW_ENUM_ICON_FILTER;
        }
    }

    if (kind == UI_PARAM_BOOL)  return UIW_SWITCH;
    if (kind == UI_PARAM_CONT)  return UIW_KNOB;
    return UIW_NONE;
}

ui_widget_type_t uiw_pick_from_kind_label_only(ui_param_kind_t kind,
                                               const char *label) {
    if (label) {
        char key[64]; normalize_label(key, sizeof key, label);
        if (strstr(key, "wave") || strstr(key, "osc"))   return UIW_ENUM_ICON_WAVE;
        if (strstr(key, "filt"))                          return UIW_ENUM_ICON_FILTER;
    }
    if (kind == UI_PARAM_BOOL)  return UIW_SWITCH;
    if (kind == UI_PARAM_CONT)  return UIW_KNOB;
    if (kind == UI_PARAM_ENUM)  return UIW_NONE;
    return UIW_NONE;
}

/* ==========================================================================
 *                              DESSINS SPÉCIFIQUES
 * ==========================================================================
 */

/**
 * @brief Dessine un interrupteur booléen uniquement via icône (ON/OFF). Pas de fallback texte.
 *
 * @note Requiert que UI_ICON_ON / UI_ICON_OFF soient définies (sinon rien).
 */
void uiw_draw_switch(int x, int y, int w, int h, bool on) {
    (void)uiw_draw_icon_by_text(on ? "on" : "off", x, y, w, h);
}

/* --- Helpers locaux pour géométrie du knob dans un cadre ------------------ */

static inline int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Calcule centre + rayon pour ui_knob, sans débordement, avec option diamètre fixe. */
static void compute_knob_geom(int x, int y, int w, int h,
                              const uiw_knob_style_t *st,
                              int *out_cx, int *out_cy, int *out_r) {
    int pad = st ? st->padding : UIW_KNOB_STYLE_DEFAULT.padding;
    if (pad < 0) pad = 0;
    int cw = w - 2 * pad; if (cw < 6) cw = 6;
    int ch = h - 2 * pad; if (ch < 6) ch = 6;

#ifdef UIW_KNOB_DIAMETER_PX
    int d = clampi(UIW_KNOB_DIAMETER_PX, 6, (cw < ch ? cw : ch));
#else
    int d = (cw < ch ? cw : ch);
#endif
    if (d & 1) d -= 1; /* pair pour centrage propre */

    *out_cx = x + w / 2;
    *out_cy = y + h / 2;
    *out_r  = d / 2;
}

/**
 * @brief Dessine un knob continu via le moteur ui_knob.c (LUT 300°), sans débordement.
 */
void uiw_draw_knob_ex(int x, int y, int w, int h,
                      int value, int min, int max,
                      const uiw_knob_style_t *style) {
    int cx, cy, r;
    compute_knob_geom(x, y, w, h, style ? style : &UIW_KNOB_STYLE_DEFAULT, &cx, &cy, &r);

    /* ui_knob.c dessine l’outline + les rayons sur un arc “Elektron-like”.
       Il gère déjà la normalisation [min..max] → 0..255, pas de débordement. */
    ui_draw_knob(cx, cy, r, value, min, max);
}

void uiw_draw_knob(int x, int y, int w, int h, int value, int min, int max) {
    uiw_draw_knob_ex(x, y, w, h, value, min, max, &UIW_KNOB_STYLE_DEFAULT);
}
