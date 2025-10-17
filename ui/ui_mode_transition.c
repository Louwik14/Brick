#include <string.h>

#include "ui/ui_mode_transition.h"
#include "ui/ui_mute_backend.h"
#include "ui/ui_overlay.h"
#include "ui/ui_input.h"

static ui_mode_transition_t s_last_transition = {
    .previous_mode = SEQ_MODE_DEFAULT,
    .next_mode     = SEQ_MODE_DEFAULT,
    .reason        = "boot",
    .ui_synced     = false,
    .led_synced    = false,
    .seq_synced    = false,
};

void ui_mode_transition_begin(ui_mode_transition_t *transition,
                              seq_mode_t previous_mode,
                              seq_mode_t next_mode,
                              const char *reason)
{
    if (!transition) {
        return;
    }

    transition->previous_mode = previous_mode;
    transition->next_mode     = next_mode;
    transition->reason        = reason;
    transition->ui_synced     = false;
    transition->led_synced    = false;
    transition->seq_synced    = false;

    s_last_transition = *transition;
    UI_MODE_TRACE("transition begin %d -> %d (%s)",
                  (int)previous_mode,
                  (int)next_mode,
                  reason ? reason : "-");
}

void ui_mode_transition_mark_ui_synced(ui_mode_transition_t *transition)
{
    if (!transition) {
        return;
    }
    transition->ui_synced = true;
    s_last_transition.ui_synced = true;
    UI_MODE_TRACE("transition ui synced %d -> %d", (int)transition->previous_mode,
                  (int)transition->next_mode);
}

void ui_mode_transition_mark_led_synced(ui_mode_transition_t *transition)
{
    if (!transition) {
        return;
    }
    transition->led_synced = true;
    s_last_transition.led_synced = true;
    UI_MODE_TRACE("transition led synced %d -> %d", (int)transition->previous_mode,
                  (int)transition->next_mode);
}

void ui_mode_transition_mark_seq_synced(ui_mode_transition_t *transition)
{
    if (!transition) {
        return;
    }
    transition->seq_synced = true;
    s_last_transition.seq_synced = true;
    UI_MODE_TRACE("transition seq synced %d -> %d", (int)transition->previous_mode,
                  (int)transition->next_mode);
}

void ui_mode_reset_context(ui_context_t *ctx, seq_mode_t next_mode)
{
    if (!ctx) {
        return;
    }

    const bool shift_pressed = ui_input_shift_is_pressed();
    ctx->mute_plus_down       = false;
    ctx->mute_shift_latched   = shift_pressed;
    ctx->track.shift_latched  = shift_pressed;

    ctx->seq.held_mask = 0U;
    memset(ctx->seq.held_flags, 0, sizeof(ctx->seq.held_flags));
    memset(ctx->seq.hold_start, 0, sizeof(ctx->seq.hold_start));

    if (next_mode != SEQ_MODE_PMUTE) {
        ctx->mute_state = UI_MUTE_STATE_OFF;
        ui_mute_backend_clear();
    }

    if (next_mode == SEQ_MODE_TRACK) {
        ctx->keyboard.active = false;
        ctx->keyboard.overlay_visible = false;
        ctx->overlay_active  = false;
        ctx->overlay_id      = UI_OVERLAY_NONE;
        ctx->overlay_submode = 0u;
    } else {
        ctx->track.active = false;
        if (!ui_overlay_is_active()) {
            ctx->overlay_active  = false;
            ctx->overlay_id      = UI_OVERLAY_NONE;
            ctx->overlay_submode = 0u;
        }
    }
}

void ui_mode_transition_commit(const ui_mode_transition_t *transition)
{
    if (!transition) {
        return;
    }
    s_last_transition = *transition;
    UI_MODE_TRACE("transition commit %d -> %d", (int)transition->previous_mode,
                  (int)transition->next_mode);
}

const ui_mode_transition_t *ui_mode_transition_last(void)
{
    return &s_last_transition;
}
