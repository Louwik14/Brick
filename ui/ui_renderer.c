/**
 * @file ui_renderer.c
 * @brief Rendu graphique principal de l’interface Brick sur OLED.
 *
 * @ingroup ui
 *
 * @details
 * Convertit l’état logique de l’UI (`ui_state_t`) en pixels :
 *  - Bandeau haut (cartouche + **label de mode custom actif** accolé à droite,
 *    titre/menu tel que défini actuellement, tempo, note, etc.)
 *  - 4 cadres param (un par encodeur)
 *  - Bandeau bas (pages)
 *
 * Invariants & architecture :
 *  - Aucune logique d’état/entrée — uniquement du **rendu**.
 *  - Ne modifie jamais le modèle (`ui_state_t`).
 *  - Le label du **mode custom actif** est fourni par `ui_backend_get_mode_label()`
 *    (ex. "SEQ", "ARP", "KEY+1"). Les overlays peuvent temporairement
 *    écraser ce label via le backend.
 *  - Accès à l’état/cart via fonctions d’accès (forward-declarées).
 *  - Rendu des widgets via `ui_widgets` (switch, icônes par TEXTE, knob).
 *
 * Hiérarchie (respectée) :
 *   ui_renderer  →  ui_widgets  →  ui_icons  →  drv_display
 */

#include "ui_renderer.h"
#include "drv_display.h"
#include "font.h"
#include "ui_widgets.h"   /* widgets modulaires (switch, icônes via label, knob) */
#include "ui_types.h"     /* ui_param_kind_t, ui_widget_type_t */
#include "ui_backend.h"   /* ui_backend_get_mode_label() */
#include "ui_overlay.h"
#include "seq_led_bridge.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ============================================================================
 *  Forward decl. pour éviter tout #include vers ui_controller.* (pas de dépendance inverse)
 * ============================================================================ */
#ifdef __cplusplus
extern "C" {
#endif

/* === Frame titre de menu (tunable) ===================================== */
#define MENU_FRAME_X   32   /* position X du cadre titre */
#define MENU_FRAME_Y    0   /* position Y du cadre titre */
#define MENU_FRAME_W   70   /* largeur du cadre titre    */
#define MENU_FRAME_H   12   /* hauteur du cadre titre    */
/* ======================================================================= */

/* État et cartouche actuels (lecture seule) */
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

static void display_draw_text_inverted(const font_t *font, uint8_t x, uint8_t y, const char *txt);
static void display_draw_text_inverted_box(const font_t *font, uint8_t x, uint8_t y, const char *txt);

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

static const int k_param_frame_width  = 31;
static const int k_param_frame_height = 37;
static const int k_param_frame_x_offsets[4] = {0, 32, 65, 97};
static const char k_note_name_table[12][3] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};
static const int k_param_frame_y = 16;

/* Largeur d’un texte en pixels */
static int text_width_px(const font_t *font, const char *s) {
    if (!font || !s) return 0;
    int n = (int)strlen(s);
    return (n <= 0) ? 0 : n * (font->width + font->spacing) - font->spacing;
}

static void _copy_project_name(const seq_project_t *project, char *dst, size_t dst_size) {
    if (!dst || dst_size == 0U) {
        return;
    }
    dst[0] = '\0';
    if (!project) {
        (void)snprintf(dst, dst_size, "%s", "PROJECT");
        return;
    }

    size_t max_len = (size_t)SEQ_PROJECT_NAME_MAX;
    size_t len = 0U;
    while ((len < max_len) && (project->name[len] != '\0')) {
        ++len;
    }

    if (len == 0U) {
        (void)snprintf(dst, dst_size, "%s", "PROJECT");
        return;
    }

    if (len >= dst_size) {
        len = dst_size - 1U;
    }

    memcpy(dst, project->name, len);
    dst[len] = '\0';
}

static void _draw_track_mode_placeholder(const seq_project_t *project,
                                         const ui_mode_context_t *ctx) {
    const uint8_t track_count = (project != NULL) ? seq_project_get_track_count(project) : 0U;
    for (int slot = 0; slot < 4; ++slot) {
        int x = k_param_frame_x_offsets[slot];
        int y = k_param_frame_y;
        draw_rect_open_corners(x, y, k_param_frame_width, k_param_frame_height);

        char label[12];
        (void)snprintf(label, sizeof(label), "CART%d", slot + 1);
        int tw_label = text_width_px(&FONT_4X6, label);
        drv_display_draw_text_with_font(&FONT_4X6,
                                        (uint8_t)(x + (k_param_frame_width - tw_label) / 2),
                                        (uint8_t)(y + 3),
                                        label);

        for (int row = 0; row < 4; ++row) {
            uint8_t track_idx = (uint8_t)(slot * 4 + row);
            const bool within_bounds = (track_idx < track_count);
            const seq_model_track_t *track_model =
                (project != NULL && within_bounds) ? seq_project_get_track_const(project, track_idx) : NULL;
            const bool present = (track_model != NULL);
            const bool active = present && ctx && (track_idx == ctx->seq.track_index);

            char line[12];
            if (!present) {
                (void)snprintf(line, sizeof(line), "--");
            } else {
                (void)snprintf(line, sizeof(line), "%cT%02u",
                               active ? '>' : ' ',
                               (unsigned)(track_idx + 1U));
            }

        int y_line = y + 10 + row * (FONT_4X6.height + 1);
            if (active) {
                int tw_line = text_width_px(&FONT_4X6, line);
                draw_filled_rect(x + 2, y_line - 1, tw_line + 2, FONT_4X6.height + 2);
                display_draw_text_inverted(&FONT_4X6,
                                           (uint8_t)(x + 3),
                                           (uint8_t)y_line,
                                           line);
            } else {
                drv_display_draw_text_with_font(&FONT_4X6,
                                                (uint8_t)(x + 3),
                                                (uint8_t)y_line,
                                                line);
            }
        }

        char bs_hint[12];
        (void)snprintf(bs_hint, sizeof(bs_hint), "BS%u-%u",
                       (unsigned)(slot * 4U + 1U),
                       (unsigned)(slot * 4U + 4U));
        int tw_hint = text_width_px(&FONT_4X6, bs_hint);
        int hint_y = y + k_param_frame_height - (FONT_4X6.height + 2);
        if (hint_y < y + 20) {
            hint_y = y + k_param_frame_height - (FONT_4X6.height + 1);
        }
        drv_display_draw_text_with_font(&FONT_4X6,
                                        (uint8_t)(x + (k_param_frame_width - tw_hint) / 2),
                                        (uint8_t)hint_y,
                                        bs_hint);
    }

    const char *exit_hint = "SHIFT+BS11 EXIT";
    int tw_exit = text_width_px(&FONT_4X6, exit_hint);
    int exit_x = (OLED_WIDTH - tw_exit) / 2;
    if (exit_x < 0) {
        exit_x = 0;
    }
    drv_display_draw_text_with_font(&FONT_4X6, (uint8_t)exit_x, 56, exit_hint);
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

static void format_note_label(int value, char *buf, size_t len) {
    if (buf == NULL || len == 0) {
        return;
    }
    if (value < 0) {
        value = 0;
    } else if (value > 127) {
        value = 127;
    }
    int octave = (value / 12) - 1;
    int pc = value % 12;
    (void)snprintf(buf, len, "%s%d", k_note_name_table[pc], octave);
}

static int hold_param_index_for_render(const ui_menu_spec_t *menu, uint8_t page, uint8_t param_idx) {
    if (!menu || !menu->name) {
        return -1;
    }
    if (strcmp(menu->name, "SEQ") != 0) {
        return -1;
    }
    if (page == 0U && param_idx < 4U) {
        return param_idx;
    }
    if (page >= 1U && page <= 4U && param_idx < 4U) {
        return (int)(SEQ_HOLD_PARAM_V1_NOTE + ((page - 1U) * 4U) + param_idx);
    }
    return -1;
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
    const seq_led_bridge_hold_view_t *hold_view = seq_led_bridge_get_hold_view();
    const bool hold_active = (hold_view != NULL) && hold_view->active && (hold_view->step_count > 0U);

    const ui_mode_context_t *mode_ctx = ui_backend_get_mode_context();
    const bool track_mode_active = (mode_ctx != NULL) && mode_ctx->track.active;
    const seq_project_t *project = seq_led_bridge_get_project_const();
    char project_name[SEQ_PROJECT_NAME_MAX + 1U];
    _copy_project_name(project, project_name, sizeof(project_name));

    /* ===== Bandeau haut ===== */

    /* 1) Numéro de cartouche, à GAUCHE en inversé */
    snprintf(buf, sizeof(buf), "%d", (int)1); /* TODO: remplace par l'ID réel si dispo */
    int tw_id = text_width_px(&FONT_5X7, buf);
    int x_id  = 1;
    display_draw_text_inverted_box(&FONT_5X7, (uint8_t)x_id, 1, buf);

    /* 2) Bloc gauche : CartName (ligne haute) + Mode custom (ligne basse) en 4x6 non inversé */
    const int x0_left = tw_id + 5;  /* petit espace après le numéro inversé */
    // --- FIX: suppression de x_left_end (non utilisé) pour éviter les warnings ---

    /* 2a) Nom de cartouche : police 4x6, non inversé, ligne du haut (baseline = 8) */
    int tw_cart = 0;
    const char *cart_name = cart->cart_name;
    const char *override_name = ui_overlay_get_banner_cart_override();
    if (override_name && override_name[0]) {
        cart_name = override_name;
    }
    if (track_mode_active && project_name[0] != '\0') {
        cart_name = project_name;
    }
    if (cart_name && cart_name[0]) {
        drv_display_draw_text_with_font(&FONT_4X6, (uint8_t)x0_left, 0, cart_name);
        tw_cart = text_width_px(&FONT_4X6, cart_name);
    }

    /* 2b) Mode custom actif persistant : police 4x6, non inversé, ligne du bas (baseline = 15) */
    const char *tag = ui_backend_get_mode_label();
    if (!tag || tag[0] == '\0') {
        const char *override_tag = ui_overlay_get_banner_tag_override();
        if (override_tag && override_tag[0]) {
            tag = override_tag;
        } else if (cart->overlay_tag && cart->overlay_tag[0]) {
            tag = cart->overlay_tag;
        }
    }

    int tw_tag = 0;
    if (tag && tag[0]) {
        drv_display_draw_text_with_font(&FONT_4X6, (uint8_t)x0_left, 8, tag);
        tw_tag = text_width_px(&FONT_4X6, tag);
    }

    /* 2c) La fenêtre de centrage part de la fin du bloc le plus large (cart vs tag) */
    /* --- x_left_end supprimé : la largeur utile est directement calculée si besoin --- */
    (void)tw_cart; // --- FIX: éviter warning unused quand aucun centrage dynamique n'est appliqué ---
    (void)tw_tag;  // --- FIX: idem pour le tag overlay ---

    /* === Titre du menu : centré entre fin (cart+tag) et zone note (~100 px) === */
    const char *menu_title = menu->name ? menu->name : "";
    if (track_mode_active && project_name[0] != '\0') {
        menu_title = project_name;
    }
    snprintf(buf, sizeof(buf), "%s", menu_title);

    /* 1) Cadre à coins ouverts (esthétique : pas de pixels aux 4 coins) */
    draw_rect_open_corners(MENU_FRAME_X, MENU_FRAME_Y, MENU_FRAME_W, MENU_FRAME_H);

    /* 2) Centrage du texte DANS le cadre (indépendant de cart/tag/note) */
    int tw_menu = text_width_px(&FONT_5X7, buf);

    /* Centre horizontal : */
    int x_menu = MENU_FRAME_X + (MENU_FRAME_W - tw_menu) / 2;
    if (x_menu < MENU_FRAME_X) x_menu = MENU_FRAME_X;

    /* Centre vertical : on utilise draw_text_with_font (coordonnée = top-left) */
    int y_menu_top = MENU_FRAME_Y + (MENU_FRAME_H - FONT_5X7.height) / 2;
    if (y_menu_top < MENU_FRAME_Y) y_menu_top = MENU_FRAME_Y;

    drv_display_draw_text_with_font(&FONT_5X7, (uint8_t)x_menu, (uint8_t)y_menu_top, buf);
    /* ======================================================================= */

    /* Icône note + BPM/PTN (inchangés) */
    draw_note_icon(101, 1);
    bool clock_external = false; // TODO: état réel
    if (clock_external)
        display_draw_text_inverted_box(&FONT_4X6, 108, 1, "120.0");
    else
        drv_display_draw_text_at_baseline(&FONT_4X6, 109, 8, "120.0");

    drv_display_draw_text_at_baseline(&FONT_4X6, 113, 15, "A-12");


    if (track_mode_active) {
        _draw_track_mode_placeholder(project, mode_ctx);
        drv_display_update();
        return;
    }

    /* ===== 4 cadres paramètres ===== */
    for (int i = 0; i < 4; i++) {
        int x = k_param_frame_x_offsets[i];
        int y = k_param_frame_y;
        draw_rect_open_corners(x, y, k_param_frame_width, k_param_frame_height);

        const ui_param_spec_t *ps = &page->params[i];
        if (!ps->label) continue;

        int hold_idx = hold_param_index_for_render(menu, st->cur_page, (uint8_t)i);
        seq_led_bridge_hold_param_t cart_hold_param;
        const seq_led_bridge_hold_param_t *hold_param =
            (hold_active && hold_idx >= 0) ? &hold_view->params[hold_idx] : NULL;
        if (hold_active && hold_idx < 0 && ((ps->dest_id & UI_DEST_MASK) == UI_DEST_CART)) {
            if (seq_led_bridge_hold_get_cart_param(UI_DEST_ID(ps->dest_id), &cart_hold_param)) {
                hold_param = &cart_hold_param;
            }
        }
        const bool hold_plocked = (hold_param != NULL) && hold_param->plocked;
        const bool hold_available = (hold_param != NULL) && hold_param->available;
        const bool hold_mixed = hold_available && hold_param->mixed;
        int32_t hold_value = hold_available ? hold_param->value : 0;

        /* --- Label param centré --- */
        int tw_label = text_width_px(&FONT_4X6, ps->label);
        int x_label = x + (k_param_frame_width - tw_label) / 2;
        if (hold_plocked) {
            draw_filled_rect(x_label - 1, y + 2, tw_label + 2, FONT_4X6.height + 2);
            display_draw_text_inverted(&FONT_4X6, x_label, y + 3, ps->label);
        } else {
            drv_display_draw_text_with_font(&FONT_4X6, x_label, y + 3, ps->label);
        }

        /* --- Valeur actuelle --- */
        const ui_param_state_t *pv =
            &st->vals.menus[st->cur_menu].pages[st->cur_page].params[i];

        char valbuf[24] = {0};
        int  knob_value = (int)pv->value;   /* valeur “numérique” pour knob fallback */
        bool bool_on    = (pv->value != 0);

        if (hold_available) {
            if (hold_mixed) {
                snprintf(valbuf, sizeof(valbuf), "--");
            } else {
                if (ps->kind == UI_PARAM_ENUM) {
                    format_note_label((int)hold_value, valbuf, sizeof(valbuf));
                } else {
                    snprintf(valbuf, sizeof(valbuf), "%d", (int)hold_value);
                }
                knob_value = (int)hold_value;
                bool_on = (hold_value != 0);
            }
        } else {
            if (ps->kind == UI_PARAM_ENUM) {
                const char *s = (pv->value < ps->meta.en.count && ps->meta.en.labels)
                                ? ps->meta.en.labels[pv->value] : "?";
                snprintf(valbuf, sizeof(valbuf), "%s", s);
            }
            else if (ps->kind == UI_PARAM_BOOL) {
                const char *s = (pv->value < ps->meta.en.count && ps->meta.en.labels)
                                ? ps->meta.en.labels[pv->value] : (pv->value ? "ON" : "OFF");
                snprintf(valbuf, sizeof(valbuf), "%s", s);
                bool_on = (pv->value != 0);
                knob_value = (int)pv->value;
            }
            else { // CONT / autre numérique
                snprintf(valbuf, sizeof(valbuf), "%d", (int)pv->value);
            }
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
            uiw_draw_switch(x, y, k_param_frame_width, k_param_frame_height, bool_on);
            break;

        case UIW_ENUM_ICON_WAVE:
        case UIW_ENUM_ICON_FILTER: {
            const char *txt = NULL;
            if (ps->kind == UI_PARAM_ENUM &&
                ps->meta.en.labels && pv->value < ps->meta.en.count) {
                txt = ps->meta.en.labels[pv->value];
            }
            /* Dessin via label réel ; si non reconnu → ne rien afficher (pas de fallback knob) */
            (void)uiw_draw_icon_by_text(txt, x, y, k_param_frame_width, k_param_frame_height);
            break;
        }

        case UIW_KNOB:
        default: {
            /* Dessiner un knob **uniquement** pour les CONT */
            if (ps->kind == UI_PARAM_CONT) {
                if (!(hold_param != NULL && !hold_available)) {
                    int v = knob_value;
                    int vmin = ps->meta.range.min;
                    int vmax = ps->meta.range.max;
                    if (vmax <= vmin) { vmin = 0; vmax = 255; }
                    uiw_draw_knob(x, y, k_param_frame_width, k_param_frame_height, v, vmin, vmax);
                }
            }
            /* ENUM/BOOL sans widget spécifique → ne rien dessiner */
        } break;
        }

        /* --- Valeur texte centrée bas --- */
        int tw_val = text_width_px(&FONT_4X6, valbuf);
        int x_val = x + (k_param_frame_width - tw_val) / 2;
        drv_display_draw_text_with_font(&FONT_4X6,
                                        x_val,
                                        y + k_param_frame_height - 8,
                                        valbuf);
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
