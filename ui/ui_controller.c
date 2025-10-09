/**
 * @file ui_controller.c
 * @brief Logique centrale de l’UI Brick : menus, pages, encodeurs + cycles BM data-driven.
 * @ingroup ui
 *
 * @details
 * - Charge les cycles BM depuis la spec UI (`ui_spec.h::ui_cart_spec_t::cycles[]`) à chaque
 *   `ui_init()` / `ui_switch_cart()`.
 * - Avance dans le cycle si l’on ré-appuie **le même BM** (souvenu via `s_current_bm`).
 * - Sélectionne l’élément courant (honore `resume`) si l’on appuie un **nouveau BM**.
 * - Le renderer obtient toujours le menu actif via `ui_resolve_menu()`.
 *
 * Invariants README :
 * - Aucune dépendance bus/UART : propagation params via `ui_backend`.
 * - Headers UI sans drivers ; mapping hard → UI confiné à `ui_input.c`.
 */

#include "ui_controller.h"
#include "ui_backend.h"      /* pont neutre vers cart_link */
#include "ui_spec.h"         /* UI_MENUS_PER_CART, UI_PAGES_PER_MENU */
#include <string.h>
#include <stdbool.h>

/* ============================================================
 * État global et dirty flag
 * ============================================================ */
static ui_state_t g_ui;
static volatile bool g_ui_dirty = true;

/* Dernière spec chargée (pour éviter des recharges de cycles inutiles) */
static const ui_cart_spec_t* s_last_spec = NULL;

/* Souvenir du **dernier BM** utilisé pour décider si l’on avance dans le cycle */
static int s_current_bm = -1;

/* ============================================================
 * Gestion des cycles de menus (BM1..BM8)
 * ============================================================ */
typedef struct {
    const ui_menu_spec_t* opts[UI_CYCLE_MAX_OPTS]; /**< pointeurs vers des menus de la spec courante */
    uint8_t count;                                 /**< nombre d’options valides */
    uint8_t idx;                                   /**< index courant dans le cycle */
    bool    resume;                                /**< si true : on reprend l’index courant au retour sur ce BM */
} ui_cycle_t;

static ui_cycle_t s_cycles[8]; /**< un cycle par BM1..BM8 */

/* ============================================================
 * Dirty flag
 * ============================================================ */
void ui_mark_dirty(void)   { g_ui_dirty = true;  }
bool ui_is_dirty(void)     { return g_ui_dirty;  }
void ui_clear_dirty(void)  { g_ui_dirty = false; }

/* ============================================================
 * Helpers cycles (guards + chargement)
 * ============================================================ */

/**
 * @brief Teste si un pointeur de menu appartient au bloc `spec->menus`.
 * @details Garantit qu’un pointeur résolu ne dépasse pas `UI_MENUS_PER_CART`.
 * @ingroup ui
 */
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

/**
 * @brief Charge les cycles depuis la spec, avec **garde-fous**.
 * @details
 *  - Tronque `count` à `UI_CYCLE_MAX_OPTS`.
 *  - Vérifie `idxs[i] < UI_MENUS_PER_CART`.
 *  - Valide chaque pointeur via `ui_menu_ptr_belongs_to_spec()`.
 * @ingroup ui
 */
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
                if (!ui_menu_ptr_belongs_to_spec(spec, pmenu)) {
                    pmenu = NULL; /* refuse tout hors-bloc */
                }
            }
            s_cycles[bm].opts[i] = pmenu; /* peut rester NULL si invalide */
        }
    }
}

/**
 * @brief Sélectionne l’entrée **courante** du cycle (sans avancer).
 * @details
 *  - Applique `resume` (sinon repart à 0).
 *  - Cherche la première entrée **valide** à partir de `idx` (wrap).
 *  - Fallback : menu homonyme du BM si tout est invalide.
 * @ingroup ui
 */
static void ui_cycles_select_current(int bm_index) {
    if (bm_index < 0 || bm_index >= 8) return;
    ui_cycle_t* c = &s_cycles[bm_index];
    if (c->count == 0) return;

    if (!c->resume) c->idx = 0;

    for (uint8_t k = 0; k < c->count; ++k) {
        uint8_t i = (uint8_t)((c->idx + k) % c->count);
        const ui_menu_spec_t* m = c->opts[i];
        if (m && ui_menu_ptr_belongs_to_spec(g_ui.spec, m)) {
            c->idx = i; /* caler sur la première valide */
            g_ui.cur_menu = (uint8_t)(m - &g_ui.spec->menus[0]);
            g_ui.cur_page = 0;
            ui_mark_dirty();
            return;
        }
    }

    /* Fallback : menu homonyme si existe */
    if (bm_index < UI_MENUS_PER_CART) {
        g_ui.cur_menu = (uint8_t)bm_index;
        g_ui.cur_page = 0;
        ui_mark_dirty();
    }
}

/**
 * @brief Avance d’une étape dans le cycle d’un BM.
 * @details Saute les entrées NULL/invalides ; fallback vers menu homonyme si tout invalide.
 * @ingroup ui
 */
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

    /* Après un tour complet sans valide → fallback */
    if (bm_index < UI_MENUS_PER_CART) {
        g_ui.cur_menu = (uint8_t)bm_index;
        g_ui.cur_page = 0;
        ui_mark_dirty();
    }
}

/* ============================================================
 * Initialisation / changement de cart
 * ============================================================ */

/**
 * @brief Initialisation de l’UI avec une spécification de cartouche.
 * @details Réinitialise l’état et (re)charge les cycles si la spec change.
 * @ingroup ui
 */
void ui_init(const ui_cart_spec_t *spec) {
    g_ui_dirty  = true;
    s_current_bm = -1;

    if (!spec) {
        memset(&g_ui, 0, sizeof(g_ui));
        g_ui.spec = NULL;
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

/**
 * @brief Bascule vers une nouvelle cartouche UI.
 * @details Réinitialise l’état et recharge les cycles de la nouvelle spec.
 * @ingroup ui
 */
void ui_switch_cart(const ui_cart_spec_t *spec) {
    g_ui_dirty = true;
    s_current_bm = -1;

    if (!spec) {
        memset(&g_ui, 0, sizeof(g_ui));
        g_ui.spec = NULL;
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
 * Accès état & résolution menu (pour renderer)
 * ============================================================ */
const ui_state_t*     ui_get_state(void)    { return &g_ui;        }
const ui_cart_spec_t* ui_get_cart(void)     { return g_ui.spec;    }

/**
 * @brief Donne le **menu actif** pour rendu.
 * @details Par design, le cycle est résolu **au press BM** (g_ui.cur_menu).
 * @ingroup ui
 */
const ui_menu_spec_t* ui_resolve_menu(uint8_t bm_index /* ignoré */) {
    (void)bm_index;
    if (!g_ui.spec) return NULL;
    return &g_ui.spec->menus[g_ui.cur_menu];
}

/* ============================================================
 * Boutons : menus & pages
 * ============================================================ */

/**
 * @brief Appui sur un bouton **menu** (BM1..BM8).
 * @details
 *  - BM avec cycle :
 *      * si **même BM** que le précédent → **avance** dans le cycle,
 *      * sinon → **sélectionne** l’élément courant (honore `resume`).
 *  - BM sans cycle : sélection simple du menu homonyme.
 * @ingroup ui
 */
void ui_on_button_menu(int index) {
    if (index < 0 || index >= 8) return;

    if (s_cycles[index].count > 0) {
        if (s_current_bm == index) {
            ui_cycles_advance(index);
        } else {
            s_current_bm = index;
            ui_cycles_select_current(index);
        }
    } else {
        if ((uint8_t)index < UI_MENUS_PER_CART) {
            g_ui.cur_menu = (uint8_t)index;
            g_ui.cur_page = 0;
            s_current_bm  = index;
            ui_mark_dirty();
        }
    }
}

/**
 * @brief Appui sur un bouton **page** (P1..P5).
 * @ingroup ui
 */
void ui_on_button_page(int index) {
    if (index < 0 || index >= UI_PAGES_PER_MENU) return;
    g_ui.cur_page = (uint8_t)index;
    ui_mark_dirty();
}

/* ============================================================
 * Encodeurs
 * ============================================================ */
static inline int clampi(int v, int mn, int mx) {
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

/**
 * @brief Mouvement d’encodeur (0..3) sur la page courante.
 * @details Applique CONT/ENUM/BOOL et propage via `ui_backend_*`.
 * @ingroup ui
 */
void ui_on_encoder(int enc_index, int delta) {
    if (enc_index < 0 || enc_index >= UI_PARAMS_PER_PAGE) return;
    if (!g_ui.spec) return;

    const ui_menu_spec_t *menu = ui_resolve_menu(g_ui.cur_menu);
    if (!menu) return;
    const ui_page_spec_t *page = &menu->pages[g_ui.cur_page];

    const ui_param_spec_t *ps = &page->params[enc_index];
    ui_param_state_t *pv = &g_ui.vals.menus[g_ui.cur_menu]
                               .pages[g_ui.cur_page]
                               .params[enc_index];

    if (!ps->label) return; /* param absent */

    switch (ps->kind) {
    case UI_PARAM_CONT: {
        int step = (ps->meta.range.step > 0) ? ps->meta.range.step : 1;
        int v = (int)pv->value + delta * step;
        v = clampi(v, ps->meta.range.min, ps->meta.range.max);
        pv->value = (uint8_t)v;
        ui_backend_param_changed(ps->dest_id, (uint8_t)v,
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
        pv->value = (uint8_t)v;
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
            if (new_bit) reg |=  ps->bit_mask;
            else         reg &= (uint8_t)~ps->bit_mask;

            ui_backend_shadow_set(ps->dest_id, reg);
            pv->value = (reg & ps->bit_mask) ? 1 : 0;
            ui_backend_param_changed(ps->dest_id, reg, true, ps->bit_mask);
        } else {
            pv->value = new_bit;
            ui_backend_param_changed(ps->dest_id, pv->value, false, 0);
        }
        ui_mark_dirty();
        break;
    }
    default: break;
    }
}
