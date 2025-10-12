/**
 * @file ui_controller.c
 * @brief Logique centrale de l’UI Brick : menus, pages, encodeurs + cycles BM.
 * @ingroup ui
 *
 * @details
 * - Navigation (menus/pages) et édition des 4 paramètres affichés.
 * - Propagation neutre via `ui_backend_param_changed`.
 * - **Hook Phase 6** : MAJ LED *immédiate* pour le paramètre
 *   `KEYBOARD → Omnichord` (quand la vitrine Keyboard est active).
 */

#include "ui_controller.h"
#include "ui_backend.h"
#include "ui_spec.h"
#include <string.h>
#include <stdbool.h>

/* Hook LEDs (Phase 6) */
#include "ui_led_backend.h"
#include "ui_keyboard_ui.h"  /* expose KBD_OMNICHORD_ID */

/* ============================================================================
 * État & dirty
 * ==========================================================================*/
static ui_state_t g_ui;
static volatile bool g_ui_dirty = true;
static const ui_cart_spec_t* s_last_spec = NULL;
static int s_current_bm = -1; /* dernier BM utilisé (pour reprise éventuelle) */

void ui_mark_dirty(void)   { g_ui_dirty = true;  }
bool ui_is_dirty(void)     { return g_ui_dirty;  }
void ui_clear_dirty(void)  { g_ui_dirty = false; }

/* ============================================================================
 * Cycles BM
 * ==========================================================================*/
typedef struct {
    const ui_menu_spec_t* opts[UI_CYCLE_MAX_OPTS];
    uint8_t count;   /* nombre d’options valides */
    uint8_t idx;     /* index courant dans opts[] */
    bool    resume;  /* si true, reprendre idx précédent au retour sur BM */
} ui_cycle_t;

static ui_cycle_t s_cycles[8];

/* Helpers pointeur→appartenance et index */
static bool ui_menu_ptr_belongs_to_spec(const ui_cart_spec_t* spec,
                                        const ui_menu_spec_t* ptr) {
    if (!spec || !ptr) return false;
    const ui_menu_spec_t* base = &spec->menus[0];
    const ui_menu_spec_t* end  = base + UI_MENUS_PER_CART;
    return (ptr >= base) && (ptr < end);
}

static int ui_menu_index_in_spec(const ui_cart_spec_t* spec,
                                 const ui_menu_spec_t* ptr) {
    if (!ui_menu_ptr_belongs_to_spec(spec, ptr)) return -1;
    return (int)(ptr - &spec->menus[0]);
}

static inline void ui_cycles_reset(void) {
    memset(s_cycles, 0, sizeof(s_cycles));
    s_current_bm = -1;
}

/* Charge depuis la spec courante les cycles BM déclarés */
static void ui_cycles_load_from_spec(const ui_cart_spec_t* spec) {
    ui_cycles_reset();
    if (!spec) return;

    for (int bm = 0; bm < 8; ++bm) {
        const ui_cycle_idx_spec_t* cx = &spec->cycles[bm];
        if (!cx->count) continue;

        uint8_t cnt = (cx->count > UI_CYCLE_MAX_OPTS) ? UI_CYCLE_MAX_OPTS : cx->count;
        s_cycles[bm].count  = cnt;
        s_cycles[bm].idx    = 0;
        s_cycles[bm].resume = cx->resume;

        for (uint8_t i = 0; i < cnt; ++i) {
            const ui_menu_spec_t* pmenu = NULL;
            uint8_t mi = cx->idxs[i];
            if (mi < UI_MENUS_PER_CART) {
                pmenu = &spec->menus[mi];
                if (!ui_menu_ptr_belongs_to_spec(spec, pmenu)) pmenu = NULL;
            }
            s_cycles[bm].opts[i] = pmenu;
        }
    }
}

/* Avance au prochain menu du cycle BM, applique dans l’état UI */
static void ui_cycles_advance(int bm) {
    if (bm < 0 || bm >= 8) return;
    ui_cycle_t* cyc = &s_cycles[bm];
    if (cyc->count == 0) return;

    /* avancer jusqu’à une entrée non nulle (bouclage sûr) */
    for (uint8_t tries = 0; tries < cyc->count; ++tries) {
        cyc->idx = (uint8_t)((cyc->idx + 1) % cyc->count);
        const ui_menu_spec_t* pm = cyc->opts[cyc->idx];
        if (pm) {
            int mi = ui_menu_index_in_spec(g_ui.spec, pm);
            if (mi >= 0 && mi < UI_MENUS_PER_CART) {
                g_ui.cur_menu = (uint8_t)mi;
                g_ui.cur_page = 0;
                s_current_bm  = bm;
                ui_mark_dirty();
                return;
            }
        }
    }
    /* si tout est nul, ne rien faire */
}

/* Sélectionne le menu courant du cycle BM (sans avancer), applique dans l’état UI */
static void ui_cycles_select_current(int bm) {
    if (bm < 0 || bm >= 8) return;
    ui_cycle_t* cyc = &s_cycles[bm];
    if (cyc->count == 0) return;

    /* si entrée nulle, tenter de trouver la première non nulle */
    uint8_t start = cyc->idx;
    for (uint8_t tries = 0; tries < cyc->count; ++tries) {
        uint8_t i = (uint8_t)((start + tries) % cyc->count);
        const ui_menu_spec_t* pm = cyc->opts[i];
        if (!pm) continue;
        int mi = ui_menu_index_in_spec(g_ui.spec, pm);
        if (mi >= 0 && mi < UI_MENUS_PER_CART) {
            cyc->idx = i; /* caler l’index réel utilisé */
            g_ui.cur_menu = (uint8_t)mi;
            g_ui.cur_page = 0;
            s_current_bm  = bm;
            ui_mark_dirty();
            return;
        }
    }
    /* si rien de valide, pas d’action */
}

/* ============================================================================
 * Init / switch
 * ==========================================================================*/
void ui_init(const ui_cart_spec_t *spec) {
    g_ui_dirty  = true;
    s_current_bm = -1;

    if (!spec) {
        memset(&g_ui, 0, sizeof(g_ui));
        s_last_spec = NULL;
        ui_cycles_reset();
        return;
    }

    ui_state_init(&g_ui, spec);

    if (spec != s_last_spec) {
        ui_cycles_load_from_spec(spec);
        s_last_spec = spec;
    }
}

void ui_switch_cart(const ui_cart_spec_t *spec) {
    g_ui_dirty = true;
    s_current_bm = -1;

    if (!spec) {
        memset(&g_ui, 0, sizeof(g_ui));
        s_last_spec = NULL;
        ui_cycles_reset();
        return;
    }

    ui_state_init(&g_ui, spec);

    if (spec != s_last_spec) {
        ui_cycles_load_from_spec(spec);
        s_last_spec = spec;
    }
}

/* ============================================================================
 * Accès rendu
 * ==========================================================================*/
const ui_state_t*     ui_get_state(void) { return &g_ui; }
const ui_cart_spec_t* ui_get_cart(void)  { return g_ui.spec; }

const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index /* ignoré */) {
    (void)bm_index;
    if (!g_ui.spec) return NULL;
    return &g_ui.spec->menus[g_ui.cur_menu];
}

/* ============================================================================
 * Boutons
 * ==========================================================================*/
void ui_on_button_menu(int index) {
    if (index < 0 || index >= 8) return;

    if (s_cycles[index].count > 0) {
        /* cycle défini sur ce BM */
        if (s_current_bm == index) {
            ui_cycles_advance(index);
        } else {
            /* première sélection : reprendre idx courant (ou 0) */
            ui_cycles_select_current(index);
        }
    } else {
        /* pas de cycle → sélection directe du menu par index */
        if ((uint8_t)index < UI_MENUS_PER_CART) {
            g_ui.cur_menu = (uint8_t)index;
            g_ui.cur_page = 0;
            s_current_bm  = index;
            ui_mark_dirty();
        }
    }
}

void ui_on_button_page(int index) {
    if (index < 0 || index >= UI_PAGES_PER_MENU) return;
    g_ui.cur_page = (uint8_t)index;
    ui_mark_dirty();
}

/* ============================================================================
 * Helpers CONT
 * ==========================================================================*/
static inline int clampi(int v, int mn, int mx) {
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static inline uint8_t ui_encode_cont_wire(const ui_param_spec_t* ps, int v) {
    const int mn = ps->meta.range.min;
    const int mx = ps->meta.range.max;
    const int span = mx - mn;
    if (span <= 0) return 0;

    if (mn >= 0 && mx <= 255) {
        if (v < mn) v = mn; else if (v > mx) v = mx;
        return (uint8_t)v;
    }
    if (span == 255) {
        int w = v - mn;
        if (w < 0) w = 0; else if (w > 255) w = 255;
        return (uint8_t)w;
    }
    int num = (v - mn) * 255;
    int w = (num + span/2) / span;
    if (w < 0) w = 0; else if (w > 255) w = 255;
    return (uint8_t)w;
}

/* ============================================================================
 * Encodeurs (Phase 6 : hook LEDs Omnichord live)
 * ==========================================================================*/
void ui_on_encoder(int enc_index, int delta) {
    if (enc_index < 0 || enc_index >= UI_PARAMS_PER_PAGE) return;
    if (!g_ui.spec) return;

    const ui_menu_spec_t *menu = ui_resolve_menu(g_ui.cur_menu);
    if (!menu) return;

    const ui_page_spec_t *page = &menu->pages[g_ui.cur_page];
    const ui_param_spec_t *ps  = &page->params[enc_index];
    ui_param_state_t *pv = &g_ui.vals.menus[g_ui.cur_menu]
                               .pages[g_ui.cur_page]
                               .params[enc_index];
    if (!ps->label) return;

    switch (ps->kind) {
    case UI_PARAM_CONT: {
        int step = (ps->meta.range.step > 0) ? ps->meta.range.step : 1;
        int v = (int)pv->value + delta * step;
        v = clampi(v, ps->meta.range.min, ps->meta.range.max);
        pv->value = (int16_t)v;

        uint8_t w = ui_encode_cont_wire(ps, v);
        ui_backend_param_changed(ps->dest_id, w, ps->is_bitwise, ps->bit_mask);
        ui_mark_dirty();
        break;
    }
    case UI_PARAM_ENUM: {
        int count = ps->meta.en.count;
        if (count <= 0) break;

        int v = (int)pv->value + delta;
        if (v < 0) v = 0;
        if (v >= count) v = count - 1;
        pv->value = (int16_t)v;

        ui_backend_param_changed(ps->dest_id, (uint8_t)v, ps->is_bitwise, ps->bit_mask);
        ui_mark_dirty();

        /* —— Hook LEDs : Omnichord (UI Keyboard) ——————————— */
        if ( (ps->dest_id & UI_DEST_MASK) == UI_DEST_UI ) {
            uint16_t local = UI_DEST_ID(ps->dest_id);
            if (local == KBD_OMNICHORD_ID) {
                /* Garantir le contexte visuel puis pousser l’état */
                ui_led_backend_set_mode(UI_LED_MODE_KEYBOARD);
                ui_led_backend_set_keyboard_omnichord(v != 0);
            }
        }
        /* ———————————————————————————————————————————————— */
        break;
    }
    case UI_PARAM_BOOL: {
        if (delta == 0) break;
        uint8_t new_bit = (delta > 0) ? 1 : 0;

        if (ps->is_bitwise) {
            uint8_t reg = ui_backend_shadow_get(ps->dest_id);
            if (new_bit) reg |= ps->bit_mask;
            else         reg &= (uint8_t)~ps->bit_mask;
            ui_backend_shadow_set(ps->dest_id, reg);
            pv->value = (reg & ps->bit_mask) ? 1 : 0;
            ui_backend_param_changed(ps->dest_id, reg, true, ps->bit_mask);
        } else {
            pv->value = (int16_t)new_bit;
            ui_backend_param_changed(ps->dest_id, (uint8_t)new_bit, false, 0);
        }
        ui_mark_dirty();
        break;
    }
    default: break;
    }
}
