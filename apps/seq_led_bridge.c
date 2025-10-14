/**
 * @file seq_led_bridge.c
 * @brief Implémentation du pont SEQ ↔ renderer (poly 4 voix, P-Lock, publish).
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */

#include <string.h>
#include "seq_led_bridge.h"

#ifndef SEQ_MAX_PAGES
#define SEQ_MAX_PAGES 16
#endif
#ifndef SEQ_DEFAULT_PAGES
#define SEQ_DEFAULT_PAGES 4
#endif

typedef struct {
    seq_step_poly_t step[16];     /* 16 steps par page */
    uint16_t        plock_mask;   /* UI-only: held mask */
} page_bank_t;

static struct {
    page_bank_t bank[SEQ_MAX_PAGES];
    uint8_t     max_pages;
    uint8_t     visible_page;
    uint16_t    total_span;
    uint16_t    preview_mask;     /* held steps */
    seq_runtime_t rt;
} g;

/* ===== Helpers ========================================================== */
static inline uint8_t _clamp_page(uint8_t p){
    if (p >= g.max_pages) p = (g.max_pages ? (g.max_pages - 1) : 0);
    return p;
}

static inline bool _step_is_active(const seq_step_poly_t* sp){
    for (uint8_t v=0; v<sp->num_voices && v<4; ++v){
        if (sp->voices[v].active && sp->voices[v].velocity > 0) return true;
    }
    return false;
}
static inline bool _step_is_param_only(const seq_step_poly_t* sp){
    if (!sp->has_param_lock) return false;
    /* has plock + all velocities == 0 */
    for (uint8_t v=0; v<sp->num_voices && v<4; ++v){
        if (sp->voices[v].active && sp->voices[v].velocity > 0) return false;
    }
    return true;
}

static void _rebuild_rt_from_bank(void){
    memset(&g.rt, 0, sizeof(g.rt));
    g.rt.visible_page        = g.visible_page;
    g.rt.steps_per_page      = 16;
    g.rt.plock_selected_mask = g.bank[g.visible_page].plock_mask; /* UI-only */

    for (uint8_t i=0;i<16;++i){
        const seq_step_poly_t* sp = &g.bank[g.visible_page].step[i];
        seq_step_state_t* st = &g.rt.steps[i];
        st->param_only = _step_is_param_only(sp);
        st->active     = (!st->param_only) && _step_is_active(sp);
        st->recorded   = st->active; /* active & recorded même couleur/état */
    }
}

/* ===== API =============================================================== */
void seq_led_bridge_init(void){
    memset(&g, 0, sizeof(g));
    g.max_pages   = SEQ_DEFAULT_PAGES;
    g.visible_page= 0;
    g.total_span  = (uint16_t)(g.max_pages * 16);
    _rebuild_rt_from_bank();
    ui_led_seq_set_total_span(g.total_span);
    ui_led_seq_update_from_app(&g.rt);
}

void seq_led_bridge_publish(void){
    _rebuild_rt_from_bank();
    ui_led_seq_set_total_span(g.total_span);
    ui_led_seq_update_from_app(&g.rt);
}

void seq_led_bridge_set_max_pages(uint8_t max_pages){
    if (max_pages == 0) max_pages = 1;
    if (max_pages > SEQ_MAX_PAGES) max_pages = SEQ_MAX_PAGES;
    g.max_pages    = max_pages;
    g.visible_page = _clamp_page(g.visible_page);
    g.total_span   = (uint16_t)(g.max_pages * 16);
    seq_led_bridge_publish();
}
void seq_led_bridge_set_total_span(uint16_t total_steps){
    if (total_steps < 16) total_steps = 16;
    if (total_steps > 256) total_steps = 256;
    uint8_t pages = (uint8_t)(total_steps / 16u);
    if (pages == 0) pages = 1;
    g.max_pages  = (pages > SEQ_MAX_PAGES) ? SEQ_MAX_PAGES : pages;
    g.total_span = (uint16_t)(g.max_pages * 16);
    g.visible_page = _clamp_page(g.visible_page);
    seq_led_bridge_publish();
}

void seq_led_bridge_page_next(void){
    if (!g.max_pages) return;
    g.visible_page = (uint8_t)((g.visible_page + 1) % g.max_pages);
    seq_led_bridge_publish();
}
void seq_led_bridge_page_prev(void){
    if (!g.max_pages) return;
    g.visible_page = (uint8_t)((g.visible_page + g.max_pages - 1) % g.max_pages);
    seq_led_bridge_publish();
}
void seq_led_bridge_set_visible_page(uint8_t page){
    g.visible_page = _clamp_page(page);
    seq_led_bridge_publish();
}

uint8_t seq_led_bridge_get_visible_page(void){
    return g.visible_page;
}

uint8_t seq_led_bridge_get_max_pages(void){
    return g.max_pages;
}

/* ===== Edition simple ==================================================== */
void seq_led_bridge_step_clear(uint8_t i){
    if (i >= 16) return;
    memset(&g.bank[g.visible_page].step[i], 0, sizeof(seq_step_poly_t));
}
void seq_led_bridge_step_set_voice(uint8_t i, uint8_t voice_idx, uint8_t pitch, uint8_t velocity){
    if (i >= 16 || voice_idx >= 4) return;
    seq_step_poly_t* sp = &g.bank[g.visible_page].step[i];
    if (voice_idx >= sp->num_voices) sp->num_voices = (uint8_t)(voice_idx+1);
    sp->voices[voice_idx].pitch    = pitch;
    sp->voices[voice_idx].velocity = velocity;
    sp->voices[voice_idx].active   = (velocity > 0);
}
void seq_led_bridge_step_set_has_plock(uint8_t i, bool on){
    if (i >= 16) return;
    g.bank[g.visible_page].step[i].has_param_lock = on;
}

void seq_led_bridge_quick_toggle_step(uint8_t i){
    if (i >= 16) return;
    seq_step_poly_t* sp = &g.bank[g.visible_page].step[i];
    const bool was_on = _step_is_active(sp) || _step_is_param_only(sp);
    if (was_on){
        /* CLEAR complet (notes + plocks) */
        seq_led_bridge_step_clear(i);
    } else {
        /* Active simple : voix1 avec vel nominale (placeholder) */
        seq_led_bridge_step_set_voice(i, 0, 60 /*C4*/, 100);
        /* pas de plock par défaut */
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_set_step_param_only(uint8_t i, bool on){
    if (i >= 16) return;
    seq_step_poly_t* sp = &g.bank[g.visible_page].step[i];
    if (on){
        /* passe en param-only : clear velocities mais conserve has_param_lock */
        sp->has_param_lock = true;
        for (uint8_t v=0; v<sp->num_voices; ++v){
            sp->voices[v].velocity = 0;
            sp->voices[v].active   = false;
        }
    } else {
        /* retire plock; si toutes vel==0 -> off */
        sp->has_param_lock = false;
    }
    seq_led_bridge_publish();
}

/* ===== Hooks transport =================================================== */
void seq_led_bridge_on_play(void){
    ui_led_seq_set_running(true);
}
void seq_led_bridge_on_stop(void){
    ui_led_seq_set_running(false);
    /* Clear preview UI-only */
    g.preview_mask = 0;
    g.bank[g.visible_page].plock_mask = 0;
    seq_led_bridge_publish();
}

/* ===== Preview P-Lock ==================================================== */
void seq_led_bridge_set_plock_mask(uint16_t mask){
    g.bank[g.visible_page].plock_mask = mask;
    seq_led_bridge_publish();
}
void seq_led_bridge_plock_add(uint8_t i){
    if (i < 16) {
        g.bank[g.visible_page].plock_mask |=  (1u << i);
        seq_led_bridge_publish();
    }
}
void seq_led_bridge_plock_remove(uint8_t i){
    if (i < 16) {
        g.bank[g.visible_page].plock_mask &= ~(1u << i);
        seq_led_bridge_publish();
    }
}
void seq_led_bridge_plock_clear(void){
    g.bank[g.visible_page].plock_mask = 0;
    seq_led_bridge_publish();
}

void seq_led_bridge_begin_plock_preview(uint16_t held_mask){
    g.preview_mask = (held_mask & 0xFFFFu);
    g.bank[g.visible_page].plock_mask = g.preview_mask; /* UI-only */
    seq_led_bridge_publish();
}

void seq_led_bridge_apply_plock_param(uint8_t param_id, int32_t delta, uint16_t held_mask){
    (void)param_id; (void)delta; /* à raccorder au moteur param */
    /* Ici on marque juste 'has_param_lock'; la valeur réelle est gérée par le moteur UI/param */
    const uint16_t mask = held_mask & 0xFFFFu;
    for (uint8_t i=0; i<16; ++i){
        if (mask & (1u << i)){
            seq_step_poly_t* sp = &g.bank[g.visible_page].step[i];
            sp->has_param_lock = true;
            /* si aucune voix active → deviendra cyan automatiquement */
        }
    }
    seq_led_bridge_publish();
}

void seq_led_bridge_end_plock_preview(void){
    g.preview_mask = 0;
    g.bank[g.visible_page].plock_mask = 0;
    seq_led_bridge_publish();
}
