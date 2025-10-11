/**
 * @file ui_controller.c
 * @brief Logique centrale de l’UI Brick : menus, pages, encodeurs + cycles BM data-driven.
 * @ingroup ui
 *
 * @details
 * Implémente la logique haut-niveau de navigation et d’édition de l’interface :
 * - Chargement et gestion des cycles BM depuis la spec (`ui_cart_spec_t::cycles[]`).
 * - Navigation entre menus/pages.
 * - Gestion des encodeurs CONT/ENUM/BOOL.
 * - Propagation neutre via `ui_backend`.
 *
 * Correction Phase 5 :
 * - Prise en charge complète des **paramètres bipolaires** :
 *   - stockage signé (int16_t) ;
 *   - clamp sur min/max réels ;
 *   - conversion UI→wire (`uint8_t`) via `ui_encode_cont_wire()`.
 * - Traversée du zéro sans wrap ;
 * - Valeurs négatives correctement rendues (le renderer reste stateless).
 *
 * Invariants :
 * - Aucune dépendance bus/UART : tout passe via `ui_backend`.
 * - Headers UI sans drivers ; mapping hard→UI confiné à `ui_input.c`.
 */

#include "ui_controller.h"
#include "ui_backend.h"
#include "ui_spec.h"
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * État global et dirty flag
 * ============================================================ */
static ui_state_t g_ui;
static volatile bool g_ui_dirty = true;
static const ui_cart_spec_t* s_last_spec = NULL;
static int s_current_bm = -1;

/* ============================================================
 * Helpers cycles menus (inchangés)
 * ============================================================ */
typedef struct {
    const ui_menu_spec_t* opts[UI_CYCLE_MAX_OPTS];
    uint8_t count;
    uint8_t idx;
    bool    resume;
} ui_cycle_t;

static ui_cycle_t s_cycles[8];

void ui_mark_dirty(void)   { g_ui_dirty = true;  }
bool ui_is_dirty(void)     { return g_ui_dirty;  }
void ui_clear_dirty(void)  { g_ui_dirty = false; }

static bool ui_menu_ptr_belongs_to_spec(const ui_cart_spec_t* spec,
                                        const ui_menu_spec_t* ptr) {
    if (!spec || !ptr) return false;
    const ui_menu_spec_t* base = &spec->menus[0];
    const ui_menu_spec_t* end  = base + UI_MENUS_PER_CART;
    return (ptr >= base) && (ptr < end);
}

static inline void ui_cycles_reset(void) {
    memset(s_cycles, 0, sizeof(s_cycles));
    s_current_bm = -1;
}

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
                if (!ui_menu_ptr_belongs_to_spec(spec, pmenu))
                    pmenu = NULL;
            }
            s_cycles[bm].opts[i] = pmenu;
        }
    }
}

static void ui_cycles_select_current(int bm_index) {
    if (bm_index < 0 || bm_index >= 8) return;
    ui_cycle_t* c = &s_cycles[bm_index];
    if (c->count == 0) return;
    if (!c->resume) c->idx = 0;
    for (uint8_t k = 0; k < c->count; ++k) {
        uint8_t i = (uint8_t)((c->idx + k) % c->count);
        const ui_menu_spec_t* m = c->opts[i];
        if (m && ui_menu_ptr_belongs_to_spec(g_ui.spec, m)) {
            c->idx = i;
            g_ui.cur_menu = (uint8_t)(m - &g_ui.spec->menus[0]);
            g_ui.cur_page = 0;
            ui_mark_dirty();
            return;
        }
    }
    if (bm_index < UI_MENUS_PER_CART) {
        g_ui.cur_menu = (uint8_t)bm_index;
        g_ui.cur_page = 0;
        ui_mark_dirty();
    }
}

static void ui_cycles_advance(int bm_index) {
    if (bm_index < 0 || bm_index >= 8) return;
    ui_cycle_t* c = &s_cycles[bm_index];
    if (c->count == 0) return;
    for (uint8_t step = 0; step < c->count; ++step) {
        c->idx = (uint8_t)((c->idx + 1) % c->count);
        const ui_menu_spec_t* m = c->opts[c->idx];
        if (m && ui_menu_ptr_belongs_to_spec(g_ui.spec, m)) {
            g_ui.cur_menu = (uint8_t)(m - &g_ui.spec->menus[0]);
            g_ui.cur_page = 0;
            ui_mark_dirty();
            return;
        }
    }
    if (bm_index < UI_MENUS_PER_CART) {
        g_ui.cur_menu = (uint8_t)bm_index;
        g_ui.cur_page = 0;
        ui_mark_dirty();
    }
}

/* ============================================================
 * Initialisation
 * ============================================================ */
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

/* ============================================================
 * Accès et rendu
 * ============================================================ */
const ui_state_t*     ui_get_state(void) { return &g_ui; }
const ui_cart_spec_t* ui_get_cart(void)  { return g_ui.spec; }

const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index /* ignoré */) {
    (void)bm_index;
    if (!g_ui.spec) return NULL;
    return &g_ui.spec->menus[g_ui.cur_menu];
}

/* ============================================================
 * Boutons
 * ============================================================ */
void ui_on_button_menu(int index) {
    if (index < 0 || index >= 8) return;
    if (s_cycles[index].count > 0) {
        if (s_current_bm == index) ui_cycles_advance(index);
        else { s_current_bm = index; ui_cycles_select_current(index); }
    } else {
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

/* ============================================================
 * Encodeurs (corrigé pour bipolarité)
 * ============================================================ */

/**
 * @brief Clamp d’un entier dans une plage [mn,mx].
 * @ingroup ui
 */
static inline int clampi(int v, int mn, int mx) {
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

/**
 * @brief Conversion UI→wire (uint8_t) pour paramètres CONT.
 * @details
 * - Si plage unipolaire (0..255) → direct.
 * - Si plage symétrique de 255 valeurs (ex. -128..+127) → offset.
 * - Sinon : échelle linéaire.
 * @ingroup ui
 */
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

/**
 * @brief Gestion des mouvements d’encodeur (0..3).
 * @details
 * Corrigé pour les paramètres CONT bipolaires :
 * - Calcul en domaine UI (int16_t) ;
 * - Clamp sur bornes signées ;
 * - Encodage via `ui_encode_cont_wire()` ;
 * - Envoi backend neutre (`uint8_t`).
 * @ingroup ui
 */
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
        ui_backend_param_changed(ps->dest_id, w,
                                 ps->is_bitwise, ps->bit_mask);
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
        ui_backend_param_changed(ps->dest_id, (uint8_t)v,
                                 ps->is_bitwise, ps->bit_mask);
        ui_mark_dirty();
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
