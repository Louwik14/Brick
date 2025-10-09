/**
 * @file ui_renderer.c
 * @brief Rendu graphique principal de l’interface Brick sur OLED.
 *
 * @ingroup ui
 *
 * @details
 * Convertit l’état logique de l’UI (`ui_state_t`) en pixels :
 *  - Bandeau haut (cartouche, tempo, note)
 *  - 4 cadres param (un par encodeur)
 *  - Bandeau bas (pages)
 *
 * Architecture :
 *  - Aucune logique d’entrée — uniquement du rendu
 *  - Ne modifie jamais le modèle (`ui_state_t`)
 *  - Accès à l’état/cart via fonctions d’accès forward-déclarées
 *  - Rendu des widgets via `ui_widgets` (switch, icônes par TEXTE, knob)
 *
 * Hiérarchie (respectée) :
 *   ui_renderer  →  ui_widgets  →  ui_icons  →  drv_display
 */

#include "ui_renderer.h"
#include "drv_display.h"
#include "font.h"
#include "ui_widgets.h"   /* widgets modulaires (switch, icônes via label, knob) */
#include "ui_types.h"     /* ui_param_kind_t, ui_widget_type_t */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 *  Forward decl. pour éviter tout #include vers ui_controller.* (pas de dépendance inverse)
 * ============================================================================ */
#ifdef __cplusplus
extern "C" {
#endif
const ui_state_t*     ui_get_state(void);
const ui_cart_spec_t* ui_get_cart(void);
/* Résolution de menu (cycles BMx) fournie par le contrôleur, utilisée en lecture seule. */
const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index);
#ifdef __cplusplus
}
#endif

/* ====================================================================== */
/*                   HELPERS BAS-NIVEAU (FRAMEBUFFER)                     */
/* ====================================================================== */

static inline void set_pixel(int x, int y, bool on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    uint8_t *buf = drv_display_get_buffer();
    const int index = x + (y >> 3) * OLED_WIDTH;
    const uint8_t mask = (uint8_t)(1u << (y & 7));
    if (on)  buf[index] |=  mask;
    else     buf[index] &= (uint8_t)~mask;
}

static void draw_hline(int x, int y, int w) {
    if (w <= 0) return;
    for (int i = 0; i < w; i++) set_pixel(x + i, y, true);
}

static void draw_vline(int x, int y, int h) {
    if (h <= 0) return;
    for (int i = 0; i < h; i++) set_pixel(x, y + i, true);
}

/* Cadres : rectangles à coins ouverts */
static void draw_rect_open_corners(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (w > 2) {
        draw_hline(x + 1,     y,         w - 2); // haut
        draw_hline(x + 1,     y + h - 1, w - 2); // bas
    }
    if (h > 2) {
        draw_vline(x,         y + 1,     h - 2); // gauche
        draw_vline(x + w - 1, y + 1,     h - 2); // droite
    }
}

/* Rectangle plein (fond pour texte inversé) */
static void draw_filled_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            set_pixel(xx, yy, true);
        }
    }
}

/* Largeur d’un texte en pixels */
static int text_width_px(const font_t *font, const char *s) {
    if (!font || !s) return 0;
    int n = (int)strlen(s);
    return (n <= 0) ? 0 : n * (font->width + font->spacing) - font->spacing;
}

/* Texte inversé (fond noir, texte blanc) */
static void display_draw_text_inverted(const font_t *font, uint8_t x, uint8_t y, const char *txt) {
    if (!font || !txt) return;
    const uint8_t adv = font->width + font->spacing;
    while (*txt && x < OLED_WIDTH) {
        char c = *txt++;
        if ((uint8_t)c < font->first || (uint8_t)c > font->last) c = '?';
        for (uint8_t col = 0; col < font->width; col++) {
            uint8_t bits = font->get_col(c, col);
            for (uint8_t row = 0; row < font->height; row++) {
                bool on = bits & (1U << row);
                set_pixel(x + col, y + row, !on); // inversion
            }
        }
        x += adv;
    }
}

/* Texte inversé avec boîte pleine + marge 1px */
static void display_draw_text_inverted_box(const font_t *font, uint8_t x, uint8_t y, const char *txt) {
    if (!font || !txt) return;
    int tw = text_width_px(font, txt);
    int h = font->height;
    draw_filled_rect(x - 1, y - 1, tw + 2, h + 2);
    display_draw_text_inverted(font, x, y, txt);
}

/* ====================================================================== */
/*                                 ICÔNES                                  */
/* ====================================================================== */

/* NOTE (5x9) pour le bandeau haut */
static const uint16_t note_icon[5] = {
    0b010000000,
    0b111000000,
    0b111000000,
    0b010000000,
    0b001111111
};

static void draw_note_icon(int x, int y) {
    for (int col = 0; col < 5; col++) {
        uint16_t bits = note_icon[col];
        for (int row = 0; row < 9; row++) {
            if (bits & (1 << row)) set_pixel(x + col, y + row, true);
        }
    }
}

/* ====================================================================== */
/*                         RENDU PRINCIPAL PAR FRAME                       */
/* ====================================================================== */

/**
 * @brief Rendu complet d’une frame à partir d’une cart et d’un état.
 *
 * @param cart  Spécification immuable de la cartouche.
 * @param st    État courant UI (indices de menu/page et valeurs).
 */
void ui_draw_frame(const ui_cart_spec_t* cart, const ui_state_t* st) {
    if (!cart || !st) return;

    drv_display_clear();

    const ui_menu_spec_t *menu = ui_resolve_menu(st->cur_menu);
    if (!menu) return;
    const ui_page_spec_t *page = &menu->pages[st->cur_page];

    char buf[32];

    /* ===== Bandeau haut ===== */
    snprintf(buf, sizeof(buf), "%d", (int)1); // ID cartouche (placeholder)
    drv_display_draw_text_at_baseline(&FONT_5X7, 0, 8, buf);

    if (cart->cart_name) {
        int tw = text_width_px(&FONT_5X7, cart->cart_name);
        int x0 = 7;
        int y0 = 1;
        draw_filled_rect(x0 - 1, y0 - 1, tw + 2, FONT_5X7.height + 2);
        display_draw_text_inverted(&FONT_5X7, x0, y0, cart->cart_name);
    }

    snprintf(buf, sizeof(buf), "%s", menu->name ? menu->name : "");
    int tw_menu = text_width_px(&FONT_4X6, buf);
    int x_menu = (OLED_WIDTH - tw_menu) / 2;
    drv_display_draw_text_at_baseline(&FONT_4X6, x_menu, 9, buf);

    draw_note_icon(101, 1);

    bool clock_external = false; // TODO: afficher selon état réel
    if (clock_external)
        display_draw_text_inverted_box(&FONT_4X6, 108, 1, "120.0");
    else
        drv_display_draw_text_at_baseline(&FONT_4X6, 109, 8, "120.0");

    drv_display_draw_text_at_baseline(&FONT_4X6, 113, 15, "A-12");

    /* ===== 4 cadres paramètres ===== */
    const int frame_w = 31, frame_h = 37;
    const int x_offsets[4] = {0, 32, 65, 97};
    const int y_frames = 16;

    for (int i = 0; i < 4; i++) {
        int x = x_offsets[i];
        int y = y_frames;
        draw_rect_open_corners(x, y, frame_w, frame_h);

        const ui_param_spec_t *ps = &page->params[i];
        if (!ps->label) continue;

        /* --- Label param centré --- */
        int tw_label = text_width_px(&FONT_4X6, ps->label);
        int x_label = x + (frame_w - tw_label) / 2;
        drv_display_draw_text_with_font(&FONT_4X6, x_label, y + 3, ps->label);

        /* --- Valeur actuelle --- */
        const ui_param_state_t *pv =
            &st->vals.menus[st->cur_menu].pages[st->cur_page].params[i];

        char valbuf[24] = {0};
        int  knob_value = (int)pv->value;   /* valeur “numérique” pour knob fallback */
        bool bool_on    = (pv->value != 0);

        if (ps->kind == UI_PARAM_ENUM) {
            const char *s = (pv->value < ps->meta.en.count && ps->meta.en.labels)
                            ? ps->meta.en.labels[pv->value] : "?";
            snprintf(valbuf, sizeof(valbuf), "%s", s);
        }
        else if (ps->kind == UI_PARAM_BOOL) {
            /* Pour le texte, on affiche ON/OFF ou label de l’énum si fourni */
            const char *s = (pv->value < ps->meta.en.count && ps->meta.en.labels)
                            ? ps->meta.en.labels[pv->value] : (pv->value ? "ON" : "OFF");
            snprintf(valbuf, sizeof(valbuf), "%s", s);
            bool_on = (pv->value != 0);
            knob_value = (int)pv->value;
        }
        else { // CONT / autre numérique
            snprintf(valbuf, sizeof(valbuf), "%d", (int)pv->value);
        }

        /* --- Sélection du widget (famille) — **texte only** --- */
        ui_widget_type_t wtype = UIW_NONE;

        if (ps->kind == UI_PARAM_ENUM) {
            wtype = uiw_pick_from_labels(
                (ui_param_kind_t)ps->kind,
                ps->label,
                ps->meta.en.labels,
                (int)ps->meta.en.count
            );
        }
        if (wtype == UIW_NONE) {
            wtype = uiw_pick_from_kind_label_only(
                (ui_param_kind_t)ps->kind,
                ps->label
            );
        }

        /* --- Rendu du widget (icônes par **TEXTE réel**, jamais par index) --- */
        switch (wtype) {
        case UIW_SWITCH:
            uiw_draw_switch(x, y, frame_w, frame_h, bool_on);
            break;

        case UIW_ENUM_ICON_WAVE:
        case UIW_ENUM_ICON_FILTER: {
            const char *txt = NULL;
            if (ps->kind == UI_PARAM_ENUM &&
                ps->meta.en.labels && pv->value < ps->meta.en.count) {
                txt = ps->meta.en.labels[pv->value];
            }
            /* Dessin via label réel ; si non reconnu → ne rien afficher (pas de fallback knob) */
            (void)uiw_draw_icon_by_text(txt, x, y, frame_w, frame_h);
            break;
        }

        case UIW_KNOB:
        default: {
            /* Dessiner un knob **uniquement** pour les CONT */
            if (ps->kind == UI_PARAM_CONT) {
                int v = (int)pv->value;
                int vmin = ps->meta.range.min;
                int vmax = ps->meta.range.max;
                if (vmax <= vmin) { vmin = 0; vmax = 255; }
                uiw_draw_knob(x, y, frame_w, frame_h, v, vmin, vmax);
            }
            /* ENUM/BOOL sans widget spécifique → ne rien dessiner */
        } break;
        }

        /* --- Valeur texte centrée bas --- */
        int tw_val = text_width_px(&FONT_4X6, valbuf);
        int x_val = x + (frame_w - tw_val) / 2;
        drv_display_draw_text_with_font(&FONT_4X6, x_val, y + frame_h - 8, valbuf);
    }

    /* ===== Bandeau bas (pages) ===== */
    int bx = 0;
    for (int pg = 0; pg < 5; pg++) {
        const char *label = menu->page_titles[pg];
        if (!label || !label[0]) label = "-";

        bool active = (pg == st->cur_page);
        int frame_w2 = (pg == 4 ? 24 : 25);
        int tw = text_width_px(&FONT_4X6, label);
        int x_label2 = bx + (frame_w2 - tw) / 2;

        if (active) {
            draw_filled_rect(x_label2 - 1, 55, tw + 2, FONT_4X6.height + 2);
            display_draw_text_inverted(&FONT_4X6, x_label2, 56, label);
        } else {
            draw_rect_open_corners(bx, 54, frame_w2, 10);
            drv_display_draw_text_with_font(&FONT_4X6, x_label2, 56, label);
        }
        bx += (pg == 4 ? 24 : 26);
    }

    drv_display_update();
}

/* ====================================================================== */
/*                          API DE RENDU SIMPLIFIÉE                       */
/* ====================================================================== */

/**
 * @brief Appelle ui_draw_frame() avec la cartouche et l’état actuels.
 */
/* fournis par ui_controller.c ; lecture seule, aucune I/O */
void ui_render(void) {
    ui_draw_frame(ui_get_cart(), ui_get_state());
}
