#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_mode_transition.h"
#include "ui/ui_shortcuts.h"
#include "ui/ui_mute_backend.h"
#include "ui/ui_input.h"
#include "tests/stubs/ch.h"

/* -------------------------------------------------------------------------- */
/* Stubs platform                                                               */
/* -------------------------------------------------------------------------- */

static systime_t g_fake_time;

systime_t chVTGetSystemTimeX(void) { return g_fake_time; }
systime_t chVTGetSystemTime(void) { return g_fake_time; }
void chThdSleepMilliseconds(uint32_t ms) { g_fake_time += ms; }
void chSysLock(void) {}
void chSysUnlock(void) {}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}

bool ui_overlay_is_active(void) { return false; }

extern bool g_stub_mute_clear_called;

static bool g_test_shift_state;

bool ui_input_shift_is_pressed(void)
{
    return g_test_shift_state;
}

static void test_pmute_transition_clears_preview(void)
{
    ui_context_t ctx;
    ui_shortcut_map_init(&ctx);

    ctx.mute_state = UI_MUTE_STATE_PMUTE;
    g_stub_mute_clear_called = false;

    ui_mode_reset_context(&ctx, SEQ_MODE_PMUTE);
    assert(ctx.mute_state == UI_MUTE_STATE_PMUTE);
    assert(g_stub_mute_clear_called == false);

    ui_mode_reset_context(&ctx, SEQ_MODE_DEFAULT);
    assert(ctx.mute_state == UI_MUTE_STATE_OFF);
    assert(g_stub_mute_clear_called == true);
}

static void test_track_entry_from_keyboard(void)
{
    ui_mode_context_t ctx;
    ui_shortcut_map_init(&ctx);

    ctx.keyboard.active = true;

    ui_input_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.has_button = true;
    evt.btn_id = UI_BTN_SEQ11;
    evt.btn_pressed = true;

    g_test_shift_state = true;
    ui_shortcut_map_result_t res = ui_shortcut_map_process(&evt, &ctx);
    g_test_shift_state = false;

    assert(res.action_count == 1U);
    assert(res.actions[0].type == UI_SHORTCUT_ACTION_ENTER_TRACK_MODE);
    assert(ctx.track.active == true);
}

static void test_track_flag_reset_on_mode_change(void)
{
    ui_context_t ctx;
    ui_shortcut_map_init(&ctx);

    ctx.track.active = true;
    ctx.mute_state = UI_MUTE_STATE_PMUTE;
    g_stub_mute_clear_called = false;

    ui_mode_reset_context(&ctx, SEQ_MODE_DEFAULT);

    assert(ctx.track.active == false);
    assert(ctx.mute_state == UI_MUTE_STATE_OFF);
    assert(g_stub_mute_clear_called == true);
}

static void test_transition_snapshot(void)
{
    ui_mode_transition_t tr;
    ui_mode_transition_begin(&tr, SEQ_MODE_DEFAULT, SEQ_MODE_TRACK, "unit");
    assert(!tr.ui_synced && !tr.led_synced && !tr.seq_synced);

    ui_mode_transition_mark_ui_synced(&tr);
    ui_mode_transition_mark_led_synced(&tr);
    ui_mode_transition_mark_seq_synced(&tr);
    ui_mode_transition_commit(&tr);

    const ui_mode_transition_t *last = ui_mode_transition_last();
    assert(last->previous_mode == SEQ_MODE_DEFAULT);
    assert(last->next_mode == SEQ_MODE_TRACK);
    assert(last->ui_synced && last->led_synced && last->seq_synced);
}

int main(void)
{
    test_pmute_transition_clears_preview();
    test_track_entry_from_keyboard();
    test_track_flag_reset_on_mode_change();
    test_transition_snapshot();

    printf("ui_mode_edgecase_tests: OK\n");
    return 0;
}
