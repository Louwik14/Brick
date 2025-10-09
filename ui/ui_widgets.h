/**
 * @file ui_widgets.h
 * @brief Widgets UI Brick — sélection de widget et primitives de rendu (icônes + knob + switch).
 *
 * @ingroup ui
 *
 * @details
 * Cette API fournit :
 *  - La **sélection** du type de widget à utiliser pour un paramètre UI
 *    (`uiw_pick_from_labels`, `uiw_pick_from_kind_label_only`).
 *  - Les **routines de dessin** nécessaires au renderer :
 *    `uiw_draw_switch`, `uiw_draw_knob`, `uiw_draw_knob_ex`.
 *  - Un utilitaire centralisé pour dessiner une **icône par label texte** :
 *    `uiw_draw_icon_by_text`.
 *

 * Principes d’architecture :
 *  - Pas de dépendance vers `ui_controller.*` ni vers des bus/cartouches.
 *  - Rendu *pixel-perfect* via le framebuffer du driver (implémentation côté .c).
 *  - Types partagés dans `ui_types.h` (aucune redéfinition locale).
 *
 * Hiérarchie recommandée :
 *   ui_renderer  →  ui_widgets  →  ui_icons  →  drv_display
 */

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>
#include "ui_types.h"   /* ui_widget_type_t, ui_param_kind_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *                          SÉLECTION DU TYPE DE WIDGET
 * ============================================================================
 */

/**
 * @brief Détermine la famille de widget à utiliser en fonction du *kind* et des labels.
 *
 * @param kind     Type logique du paramètre (bool/enum/continu), cf. `ui_param_kind_t`.
 * @param label    Label principal du paramètre (peut aider à détecter “Wave”, “Filter”…).
 * @param labels   Tableau des labels d’énum (peut être NULL).
 * @param nlabels  Taille du tableau `labels`.
 *
 * @return `ui_widget_type_t` — famille de widget à utiliser (ou `UIW_NONE` si indéterminé).
 *
 * @note Fonction pure, non bloquante : aucune I/O ni dépendance au contrôleur.
 */
ui_widget_type_t uiw_pick_from_labels(ui_param_kind_t kind,
                                      const char *label,
                                      const char * const *labels,
                                      int nlabels);

/**
 * @brief Fallback de sélection quand on ne dispose que du *kind* et d’un label simple.
 *
 * @param kind     Type logique du paramètre (bool/enum/continu).
 * @param label    Label du paramètre (peut être NULL).
 *
 * @return `ui_widget_type_t` — famille de widget (ou `UIW_NONE`).
 */
ui_widget_type_t uiw_pick_from_kind_label_only(ui_param_kind_t kind,
                                               const char *label);

/* ============================================================================
 *                                  DESSIN
 * ============================================================================
 */

/**
 * @brief Dessine un interrupteur booléen (ON/OFF) dans un cadre.
 *
 * @param x,y  Position du coin supérieur gauche.
 * @param w,h  Dimensions du cadre.
 * @param on   État logique (true = ON).
 */
void uiw_draw_switch(int x, int y, int w, int h, bool on);

/**
 * @brief Dessine un knob circulaire pour une valeur continue.
 *
 * @param x,y   Position du coin supérieur gauche du cadre.
 * @param w,h   Dimensions du cadre.
 * @param value Valeur actuelle.
 * @param min   Borne minimale.
 * @param max   Borne maximale (si `max <= min`, une borne sûre sera utilisée).
 */
void uiw_draw_knob(int x, int y, int w, int h, int value, int min, int max);

/* ============================================================================
 *                       UTILITAIRE ICÔNES PAR LABEL TEXTE
 * ============================================================================
 */

/**
 * @brief Dessine une icône (20×14) centrée dans le cadre, choisie par **label texte**.
 *
 * @param text Label texte (insensible à la casse, espaces/underscores/tirets ignorés).
 * @param x,y  Position du coin supérieur gauche du cadre.
 * @param w,h  Dimensions du cadre.
 *
 * @return `true` si une icône reconnue a été dessinée, sinon `false`.
 *
 * @note La correspondance typique inclut : "sine", "square", "tri/triangle",
 *       "saw/sawu/sawd", "noise", "lp/hp/bp/notch".
 */
bool uiw_draw_icon_by_text(const char *text, int x, int y, int w, int h);

/** Modes de knob. */
typedef enum {
    UIW_KNOB_UNIPOLAR = 0,          /**< min >= 0 → 0 au SUD, remplissage 0→360°. */
    UIW_KNOB_BIPOLAR_ZERO_NORTH     /**< min < 0 < max → 0 au NORD ; négatif horaire / positif anti-horaire. */
} uiw_knob_mode_t;

/** Style de rendu du knob (taille/épaisseur). */
typedef struct {
    uint8_t  padding;        /**< Marge intérieure en px. */
    uint8_t  ring_thickness; /**< Épaisseur de l’anneau (0 = disque plein). */
    uint16_t fill_steps;     /**< Granularité de remplissage (lignes radiales). */
} uiw_knob_style_t;

/** Style par défaut (défini dans `ui_widgets.c`). */
extern const uiw_knob_style_t UIW_KNOB_STYLE_DEFAULT;

/**
 * @brief Variante avancée avec style configurable (taille/épaisseur).
 *
 * @param x,y,w,h  Cadre.
 * @param value    Valeur courante.
 * @param min,max  Bornes (min peut être < 0).
 * @param style    Style (si NULL → UIW_KNOB_STYLE_DEFAULT).
 */
void uiw_draw_knob_ex(int x, int y, int w, int h,
                      int value, int min, int max,
                      const uiw_knob_style_t *style);

#ifdef __cplusplus
}
#endif

#endif /* UI_WIDGETS_H */
