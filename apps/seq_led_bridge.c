/**
 * @file seq_led_bridge.c
 * @brief LED bridge consuming seq_runtime snapshots and projecting them to the UI.
 * @ingroup ui_led_backend
 * @ingroup ui_seq
 */

#include "seq_led_bridge.h"

#include <string.h>

#include "seq_runtime.h"
#include "seq_engine.h"

#ifndef SEQ_DEFAULT_PAGES
#define SEQ_DEFAULT_PAGES 4U
#endif

static struct {
    uint8_t              visible_page;
    uint8_t              max_pages;
    uint16_t             total_span;
    uint16_t             preview_mask;
    ui_seq_led_surface_t surface;
} g_bridge;

static inline uint8_t clamp_page(uint8_t page) {
    if (g_bridge.max_pages == 0) {
        return 0;
    }
    if (page >= g_bridge.max_pages) {
        page = (uint8_t)(g_bridge.max_pages - 1u);
    }
    return page;
}

static bool runtime_step_has_plock(const seq_runtime_t *rt, uint32_t abs_step) {
    for (uint8_t param = 0; param < SEQ_PARAM_COUNT; ++param) {
        if (seq_runtime_step_param_is_plocked(rt, abs_step, param)) {
            return true;
        }
    }
    return false;
}

static void rebuild_surface(void) {
    const seq_runtime_t *rt = seq_runtime_get_snapshot();
    if (!rt) {
        return;
    }

    memset(&g_bridge.surface, 0, sizeof(g_bridge.surface));
    g_bridge.surface.visible_page        = g_bridge.visible_page;
    g_bridge.surface.steps_per_page      = 16;
    g_bridge.surface.plock_selected_mask = g_bridge.preview_mask;

    const uint32_t base_step = (uint32_t)g_bridge.visible_page * 16u;
    for (uint8_t i = 0; i < 16; ++i) {
        const uint32_t abs_step = base_step + i;
        if (abs_step >= SEQ_MODEL_STEP_COUNT) {
            continue;
        }
        const bool active    = seq_runtime_step_has_note(rt, abs_step);
        const bool has_plock = runtime_step_has_plock(rt, abs_step);
        ui_seq_led_step_t *dst = &g_bridge.surface.steps[i];
        dst->active     = active;
        dst->recorded   = active;
        dst->param_only = (!active && has_plock) || ((g_bridge.preview_mask & (1u << i)) != 0u);
    }

    ui_led_seq_set_total_span(g_bridge.total_span);
    ui_led_seq_update_from_app(&g_bridge.surface);
}

void seq_led_bridge_init(void) {
    memset(&g_bridge, 0, sizeof(g_bridge));
    g_bridge.max_pages  = SEQ_DEFAULT_PAGES;
    g_bridge.total_span = (uint16_t)(g_bridge.max_pages * 16u);
    g_bridge.visible_page = 0;
    seq_led_bridge_publish();
}

void seq_led_bridge_publish(void) {
    rebuild_surface();
}

void seq_led_bridge_set_max_pages(uint8_t max_pages) {
    if (max_pages == 0) {
        max_pages = 1;
    }
    g_bridge.max_pages  = max_pages;
    g_bridge.total_span = (uint16_t)(g_bridge.max_pages * 16u);
    g_bridge.visible_page = clamp_page(g_bridge.visible_page);
    seq_led_bridge_publish();
}

void seq_led_bridge_set_total_span(uint16_t total_steps) {
    if (total_steps < 16) {
        total_steps = 16;
    }
    if (total_steps > SEQ_MODEL_STEP_COUNT) {
        total_steps = SEQ_MODEL_STEP_COUNT;
    }
    uint8_t pages = (uint8_t)((total_steps + 15u) / 16u);
    if (pages == 0) {
        pages = 1;
    }
    g_bridge.max_pages  = pages;
    g_bridge.total_span = (uint16_t)(pages * 16u);
    g_bridge.visible_page = clamp_page(g_bridge.visible_page);
    seq_led_bridge_publish();
}

void seq_led_bridge_page_next(void) {
    if (g_bridge.max_pages == 0) {
        return;
    }
    g_bridge.visible_page = (uint8_t)((g_bridge.visible_page + 1u) % g_bridge.max_pages);
    seq_led_bridge_publish();
}

void seq_led_bridge_page_prev(void) {
    if (g_bridge.max_pages == 0) {
        return;
    }
    g_bridge.visible_page = (uint8_t)((g_bridge.visible_page + g_bridge.max_pages - 1u) % g_bridge.max_pages);
    seq_led_bridge_publish();
}

void seq_led_bridge_set_visible_page(uint8_t page) {
    g_bridge.visible_page = clamp_page(page);
    seq_led_bridge_publish();
}

static uint64_t mask_for_current_page(uint16_t local_mask) {
    uint64_t mask = 0;
    const uint32_t base_step = (uint32_t)g_bridge.visible_page * 16u;
    for (uint8_t i = 0; i < 16; ++i) {
        if (local_mask & (1u << i)) {
            uint32_t abs = base_step + i;
            if (abs < SEQ_MODEL_STEP_COUNT) {
                mask |= (1ull << abs);
            }
        }
    }
    return mask;
}

void seq_led_bridge_quick_toggle_step(uint8_t index) {
    if (index >= 16) {
        return;
    }
    uint32_t abs_step = (uint32_t)g_bridge.visible_page * 16u + index;
    seq_engine_toggle_step(SEQ_MODEL_VOICE_COUNT, abs_step);
    seq_led_bridge_publish();
}

void seq_led_bridge_set_step_param_only(uint8_t index, bool on) {
    if (index >= 16) {
        return;
    }
    uint32_t abs_step = (uint32_t)g_bridge.visible_page * 16u + index;
    seq_engine_set_step_param_only(SEQ_MODEL_VOICE_COUNT, abs_step, on);
    seq_led_bridge_publish();
}

void seq_led_bridge_on_play(void) {
    seq_engine_transport_start();
    ui_led_seq_set_running(true);
    g_bridge.preview_mask = 0;
    seq_led_bridge_publish();
}

void seq_led_bridge_on_stop(void) {
    seq_engine_transport_stop();
    ui_led_seq_set_running(false);
    g_bridge.preview_mask = 0;
    seq_led_bridge_publish();
}

void seq_led_bridge_set_plock_mask(uint16_t mask) {
    g_bridge.preview_mask = mask;
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_add(uint8_t index) {
    if (index >= 16) {
        return;
    }
    g_bridge.preview_mask |= (uint16_t)(1u << index);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_remove(uint8_t index) {
    if (index >= 16) {
        return;
    }
    g_bridge.preview_mask &= (uint16_t)~(1u << index);
    seq_led_bridge_publish();
}

void seq_led_bridge_plock_clear(void) {
    g_bridge.preview_mask = 0;
    seq_led_bridge_publish();
}

void seq_led_bridge_begin_plock_preview(uint16_t held_mask) {
    g_bridge.preview_mask = held_mask;
    seq_led_bridge_publish();
}

static seq_param_id_t map_param_slot(uint8_t slot) {
    switch (slot) {
        case 0: return SEQ_PARAM_NOTE;
        case 1: return SEQ_PARAM_VELOCITY;
        case 2: return SEQ_PARAM_LENGTH;
        case 3: return SEQ_PARAM_MICRO_TIMING;
        default: return SEQ_PARAM_NOTE;
    }
}

void seq_led_bridge_apply_plock_param(uint8_t param_id, int32_t delta, uint16_t held_mask) {
    if (delta == 0) {
        return;
    }
    seq_param_id_t param = map_param_slot(param_id);
    uint64_t abs_mask = mask_for_current_page(held_mask);
    seq_engine_apply_plock_delta(param, (int16_t)delta, abs_mask);
    g_bridge.preview_mask = held_mask;
    seq_led_bridge_publish();
}

void seq_led_bridge_end_plock_preview(void) {
    g_bridge.preview_mask = 0;
    seq_led_bridge_publish();
}

uint16_t seq_led_bridge_get_preview_mask(void) {
    // FIX: expose le masque courant pour que l’UI sache quels steps éditer en P-Lock.
    return g_bridge.preview_mask;
}
