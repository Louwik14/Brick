/**
 * @file ui_shortcuts.c
 * @brief Couche de mapping pure (évènement → actions) pour les raccourcis UI.
 * @ingroup ui_shortcuts
 */

#include <string.h>

#include "ch.h"
#include "ui_shortcuts.h"

/* ========================================================================== */
/* Constantes locales                                                         */
/* ========================================================================== */
#ifndef SEQ_LONG_PRESS_MS
#define SEQ_LONG_PRESS_MS (500)
#endif

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

static inline bool _is_seq_pad(uint8_t btn) {
    return (btn >= UI_BTN_SEQ1) && (btn <= UI_BTN_SEQ16);
}

static inline uint8_t _seq_index(uint8_t btn) {
    return (uint8_t)(btn - UI_BTN_SEQ1); /* 0..15 */
}

static ui_shortcut_action_t *
_push_action(ui_shortcut_map_result_t *res, ui_shortcut_action_type_t type) {
    if (!res || res->action_count >= UI_SHORTCUT_MAX_ACTIONS) {
        return NULL;
    }
    ui_shortcut_action_t *act = &res->actions[res->action_count++];
    memset(act, 0, sizeof(*act));
    act->type = type;
    return act;
}

static bool _map_mute(const ui_input_event_t *evt,
                      ui_mode_context_t *ctx,
                      ui_shortcut_map_result_t *res,
                      bool shift_now,
                      bool shift_prev) {
    bool consumed = false;

    if (ctx->mute_state == UI_MUTE_STATE_QUICK &&
        ctx->mute_plus_down && shift_now && !shift_prev) {
        (void)_push_action(res, UI_SHORTCUT_ACTION_ENTER_MUTE_PMUTE);
        res->consumed = true;
        ctx->mute_shift_latched = shift_now;
        return true;
    }

    if (!evt->has_button) {
        ctx->mute_shift_latched = shift_now;
        return false;
    }

    if (evt->btn_id == UI_BTN_PLUS) {
        ctx->mute_plus_down = evt->btn_pressed;
    }

    if (ctx->mute_state == UI_MUTE_STATE_OFF) {
        if (evt->btn_id == UI_BTN_PLUS && evt->btn_pressed && shift_now) {
            (void)_push_action(res, UI_SHORTCUT_ACTION_ENTER_MUTE_QUICK);
            res->consumed = true;
            ctx->mute_shift_latched = shift_now;
            return true;
        }
        ctx->mute_shift_latched = shift_now;
        return false;
    }

    if (ctx->mute_state == UI_MUTE_STATE_QUICK) {
        if (_is_seq_pad(evt->btn_id)) {
            ui_shortcut_action_t *act =
                _push_action(res, UI_SHORTCUT_ACTION_TOGGLE_MUTE_TRACK);
            if (act) {
                act->data.mute.track = _seq_index(evt->btn_id);
            }
            res->consumed = true;
            consumed = true;
        } else if (evt->btn_id == UI_BTN_PLUS && !evt->btn_pressed) {
            (void)_push_action(res, UI_SHORTCUT_ACTION_EXIT_MUTE);
            res->consumed = true;
            consumed = true;
        }
    } else if (ctx->mute_state == UI_MUTE_STATE_PMUTE) {
        if (_is_seq_pad(evt->btn_id)) {
            ui_shortcut_action_t *act =
                _push_action(res, UI_SHORTCUT_ACTION_PREPARE_PMUTE_TRACK);
            if (act) {
                act->data.mute.track = _seq_index(evt->btn_id);
            }
            res->consumed = true;
            consumed = true;
        } else if (evt->btn_id == UI_BTN_PLUS && evt->btn_pressed && !shift_now) {
            (void)_push_action(res, UI_SHORTCUT_ACTION_COMMIT_PMUTE);
            res->consumed = true;
            consumed = true;
        }
    }

    ctx->mute_shift_latched = shift_now;
    return consumed;
}

static bool _map_overlays(const ui_input_event_t *evt,
                          const ui_mode_context_t *ctx,
                          ui_shortcut_map_result_t *res,
                          bool shift_now) {
    if (ctx->mute_state != UI_MUTE_STATE_OFF) {
        return false;
    }
    if (!evt->has_button || !evt->btn_pressed || !shift_now) {
        return false;
    }

    switch (evt->btn_id) {
    case UI_BTN_SEQ9:
        (void)_push_action(res, UI_SHORTCUT_ACTION_OPEN_SEQ_OVERLAY);
        res->consumed = true;
        return true;
    case UI_BTN_SEQ10:
        (void)_push_action(res, UI_SHORTCUT_ACTION_OPEN_ARP_OVERLAY);
        res->consumed = true;
        return true;
    case UI_BTN_SEQ11:
        (void)_push_action(res, UI_SHORTCUT_ACTION_OPEN_KBD_OVERLAY);
        res->consumed = true;
        return true;
    default:
        return false;
    }
}

static bool _map_keyboard_octave(const ui_input_event_t *evt,
                                 const ui_mode_context_t *ctx,
                                 ui_shortcut_map_result_t *res,
                                 bool shift_now) {
    if (ctx->mute_state != UI_MUTE_STATE_OFF) {
        return false;
    }
    if (!ctx->keyboard.active) {
        return false;
    }
    if (!evt->has_button || !evt->btn_pressed || shift_now) {
        return false;
    }

    if (evt->btn_id == UI_BTN_PLUS) {
        (void)_push_action(res, UI_SHORTCUT_ACTION_KEY_OCTAVE_UP);
        res->consumed = true;
        return true;
    }
    if (evt->btn_id == UI_BTN_MINUS) {
        (void)_push_action(res, UI_SHORTCUT_ACTION_KEY_OCTAVE_DOWN);
        res->consumed = true;
        return true;
    }
    return false;
}

static bool _map_transport(const ui_input_event_t *evt,
                           ui_shortcut_map_result_t *res,
                           bool shift_now) {
    if (!evt->has_button || !evt->btn_pressed) {
        return false;
    }
    if (shift_now) {
        return false;
    }

    switch (evt->btn_id) {
    case UI_BTN_PLAY:
        (void)_push_action(res, UI_SHORTCUT_ACTION_TRANSPORT_PLAY);
        res->consumed = true;
        return true;
    case UI_BTN_STOP:
        (void)_push_action(res, UI_SHORTCUT_ACTION_TRANSPORT_STOP);
        res->consumed = true;
        return true;
    case UI_BTN_REC:
        (void)_push_action(res, UI_SHORTCUT_ACTION_TRANSPORT_REC_TOGGLE);
        res->consumed = true;
        return true;
    default:
        return false;
    }
}

static bool _map_seq_pages(const ui_input_event_t *evt,
                           const ui_mode_context_t *ctx,
                           ui_shortcut_map_result_t *res,
                           bool shift_now) {
    if (ctx->mute_state != UI_MUTE_STATE_OFF) {
        return false;
    }
    if (ctx->keyboard.active) {
        return false;
    }
    if (!evt->has_button || !evt->btn_pressed || shift_now) {
        return false;
    }

    if (evt->btn_id == UI_BTN_PLUS) {
        (void)_push_action(res, UI_SHORTCUT_ACTION_SEQ_PAGE_NEXT);
        res->consumed = true;
        return true;
    }
    if (evt->btn_id == UI_BTN_MINUS) {
        (void)_push_action(res, UI_SHORTCUT_ACTION_SEQ_PAGE_PREV);
        res->consumed = true;
        return true;
    }
    return false;
}

static bool _map_seq_pads(const ui_input_event_t *evt,
                          ui_mode_context_t *ctx,
                          ui_shortcut_map_result_t *res) {
    if (ctx->mute_state != UI_MUTE_STATE_OFF) {
        return false;
    }
    if (ctx->keyboard.active) {
        return false;
    }
    if (!evt->has_button || !_is_seq_pad(evt->btn_id)) {
        return false;
    }

    const uint8_t idx = _seq_index(evt->btn_id);
    if (idx >= 16) {
        return false;
    }

    if (evt->btn_pressed) {
        ctx->seq.held_flags[idx] = true;
        ctx->seq.held_mask |= (uint16_t)(1u << idx);
        ctx->seq.hold_start[idx] = chVTGetSystemTimeX();

        ui_shortcut_action_t *act =
            _push_action(res, UI_SHORTCUT_ACTION_SEQ_STEP_HOLD);
        if (act) {
            act->data.seq_step.index = idx;
        }
        res->consumed = true;
    } else {
        bool was_down = ctx->seq.held_flags[idx];
        ctx->seq.held_flags[idx] = false;
        ctx->seq.held_mask &= (uint16_t)~(1u << idx);

        if (was_down) {
            const systime_t start = ctx->seq.hold_start[idx];
            const systime_t now = chVTGetSystemTimeX();
            const systime_t dt = now - start;
            const bool long_press = (dt >= TIME_MS2I(SEQ_LONG_PRESS_MS));

            ui_shortcut_action_t *act =
                _push_action(res, UI_SHORTCUT_ACTION_SEQ_STEP_RELEASE);
            if (act) {
                act->data.seq_step.index = idx;
                act->data.seq_step.long_press = long_press;
            }
        }
        res->consumed = true;
    }

    return true;
}

/* ========================================================================== */
/* API publique                                                               */
/* ========================================================================== */

void ui_shortcut_map_init(ui_mode_context_t *ctx) {
    if (!ctx) {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->custom_mode        = UI_CUSTOM_NONE;
    ctx->overlay_id         = UI_OVERLAY_NONE;
    ctx->overlay_submode    = 0u;
    ctx->overlay_active     = false;
    ctx->mute_state         = UI_MUTE_STATE_OFF;
    ctx->mute_plus_down     = false;
    ctx->mute_shift_latched = ui_input_shift_is_pressed();
    ctx->transport.playing  = false;
    ctx->transport.recording= false;
    ctx->seq.page_index     = 0u;
    ctx->seq.page_count     = 0u;
    ctx->seq.held_mask      = 0u;
    for (uint8_t i = 0; i < 16; ++i) {
        ctx->seq.held_flags[i] = false;
        ctx->seq.hold_start[i] = 0;
    }
    ctx->keyboard.active         = false;
    ctx->keyboard.overlay_visible = false;
    ctx->keyboard.octave          = 0;
}

void ui_shortcut_map_reset(ui_mode_context_t *ctx) {
    ui_shortcut_map_init(ctx);
}

ui_shortcut_map_result_t
ui_shortcut_map_process(const ui_input_event_t *evt, ui_mode_context_t *ctx) {
    ui_shortcut_map_result_t res;
    memset(&res, 0, sizeof(res));

    if (!evt || !ctx) {
        return res;
    }

    const bool shift_now  = ui_input_shift_is_pressed();
    const bool shift_prev = ctx->mute_shift_latched;

    if (_map_mute(evt, ctx, &res, shift_now, shift_prev)) {
        return res;
    }

    if (_map_overlays(evt, ctx, &res, shift_now)) {
        ctx->mute_shift_latched = shift_now;
        return res;
    }

    if (_map_keyboard_octave(evt, ctx, &res, shift_now)) {
        ctx->mute_shift_latched = shift_now;
        return res;
    }

    if (_map_transport(evt, &res, shift_now)) {
        ctx->mute_shift_latched = shift_now;
        return res;
    }

    if (_map_seq_pages(evt, ctx, &res, shift_now)) {
        ctx->mute_shift_latched = shift_now;
        return res;
    }

    if (_map_seq_pads(evt, ctx, &res)) {
        ctx->mute_shift_latched = shift_now;
        return res;
    }

    ctx->mute_shift_latched = shift_now;

    if (evt->has_encoder && evt->enc_delta != 0 &&
        ctx->mute_state == UI_MUTE_STATE_OFF &&
        !ctx->keyboard.active && ctx->seq.held_mask != 0u) {
        ui_shortcut_action_t *act =
            _push_action(&res, UI_SHORTCUT_ACTION_SEQ_ENCODER_TOUCH);
        if (act) {
            act->data.seq_mask.mask = ctx->seq.held_mask;
        }
    }

    return res;
}
