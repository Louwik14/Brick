#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_shortcuts.h"
#include "apps/seq_led_bridge.h"
#include "core/seq/seq_access.h"
#include "ui/ui_led_backend.h"
#include "ui/ui_input.h"
#include "tests/stubs/ch.h"

/* -------------------------------------------------------------------------- */
/* Stubs for LED backend                                                      */
/* -------------------------------------------------------------------------- */

static bool g_stub_track_present[SEQ_PROJECT_MAX_TRACKS];
static uint8_t g_stub_cart_counts[4];
static uint8_t g_stub_track_focus;
static ui_led_mode_t g_stub_led_mode;

void ui_led_backend_init(void) {}
void ui_led_backend_post_event(ui_led_event_t event, uint8_t index, bool state)
{
    (void)event; (void)index; (void)state;
}
void ui_led_backend_post_event_i(ui_led_event_t event, uint8_t index, bool state)
{
    (void)event; (void)index; (void)state;
}
void ui_led_backend_refresh(void) {}
void ui_led_backend_set_record_mode(bool active) { (void)active; }
void ui_led_backend_set_mode(ui_led_mode_t mode) { g_stub_led_mode = mode; }
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks)
{
    if (cart_idx < 4U) {
        g_stub_cart_counts[cart_idx] = tracks;
    }
}
void ui_led_backend_set_keyboard_omnichord(bool enabled) { (void)enabled; }
void ui_led_backend_set_track_focus(uint8_t track_index) { g_stub_track_focus = track_index; }
void ui_led_backend_set_track_present(uint8_t track_index, bool present)
{
    if (track_index < SEQ_PROJECT_MAX_TRACKS) {
        g_stub_track_present[track_index] = present;
    }
}

/* -------------------------------------------------------------------------- */
/* Timing stubs                                                                */
/* -------------------------------------------------------------------------- */

static systime_t g_fake_time;

systime_t chVTGetSystemTimeX(void) { return g_fake_time; }
systime_t chVTGetSystemTime(void) { return g_fake_time; }
void chThdSleepMilliseconds(uint32_t ms) { g_fake_time += ms; }
void chSysLock(void) {}
void chSysUnlock(void) {}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}

/* -------------------------------------------------------------------------- */
/* Stubs for shift state                                                      */
/* -------------------------------------------------------------------------- */

static bool g_shift_pressed;

bool ui_input_shift_is_pressed(void) { return g_shift_pressed; }

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static void reset_led_state(void)
{
    memset(g_stub_track_present, 0, sizeof(g_stub_track_present));
    memset(g_stub_cart_counts, 0, sizeof(g_stub_cart_counts));
    g_stub_track_focus = 0U;
    g_stub_led_mode = UI_LED_MODE_NONE;
}

static void test_track_metadata_initialisation(void)
{
    reset_led_state();
    seq_runtime_init();
    seq_led_bridge_init();
    seq_project_t *project = seq_runtime_access_project_mut();
    if (project != NULL) {
        uint8_t active_bank = seq_project_get_active_bank(project);
        uint8_t active_pattern = seq_project_get_active_pattern_index(project);
        seq_led_bridge_set_active(active_bank, active_pattern);
    } else {
        seq_led_bridge_set_active(0U, 0U);
    }
    seq_led_bridge_bind_project(project);

    /* After init the first tracks (capacity) are available, others off. */
    assert(g_stub_track_present[0] == true);
    assert(g_stub_track_focus == 0U);

    for (uint8_t track = 1U; track < SEQ_PROJECT_MAX_TRACKS; ++track) {
        if (track < seq_led_bridge_get_track_count()) {
            assert(g_stub_track_present[track] == true);
        } else {
            assert(g_stub_track_present[track] == false);
        }
    }

    /* Cart 1 exposes the contiguous number of assigned tracks, others zero. */
    assert(g_stub_cart_counts[0] == seq_led_bridge_get_track_count());
    assert(g_stub_cart_counts[1] == 0U);
    assert(g_stub_cart_counts[2] == 0U);
    assert(g_stub_cart_counts[3] == 0U);
}

static void test_track_select_focus_updates(void)
{
    reset_led_state();
    seq_runtime_init();
    seq_led_bridge_init();
    seq_project_t *project = seq_runtime_access_project_mut();
    if (project != NULL) {
        uint8_t active_bank = seq_project_get_active_bank(project);
        uint8_t active_pattern = seq_project_get_active_pattern_index(project);
        seq_led_bridge_set_active(active_bank, active_pattern);
    } else {
        seq_led_bridge_set_active(0U, 0U);
    }
    seq_led_bridge_bind_project(project);
    g_stub_track_focus = 0xFFU;

    assert(seq_led_bridge_select_track(0U) == true);
    assert(g_stub_track_focus == 0U);

    if (seq_led_bridge_get_track_count() > 1U) {
        assert(seq_led_bridge_select_track(1U) == true);
        assert(g_stub_track_focus == 1U);
    }

    /* Out of range selection leaves focus unchanged. */
    assert(seq_led_bridge_select_track(15U) == false);
    if (seq_led_bridge_get_track_count() > 1U) {
        assert(g_stub_track_focus == 1U);
    } else {
        assert(g_stub_track_focus == 0U);
    }
}

static void test_track_mode_mapping(void)
{
    ui_mode_context_t ctx;
    ui_shortcut_map_init(&ctx);

    ui_input_event_t evt;
    memset(&evt, 0, sizeof(evt));

    /* Enter track mode with SHIFT+BS11. */
    g_shift_pressed = true;
    evt.has_button = true;
    evt.btn_id = UI_BTN_SEQ11;
    evt.btn_pressed = true;
    ui_shortcut_map_result_t res = ui_shortcut_map_process(&evt, &ctx);
    assert(res.action_count == 1U);
    assert(res.actions[0].type == UI_SHORTCUT_ACTION_ENTER_TRACK_MODE);
    assert(ctx.track.active == true);

    /* Select a track while SHIFT released. */
    g_shift_pressed = false;
    memset(&evt, 0, sizeof(evt));
    evt.has_button = true;
    evt.btn_id = UI_BTN_SEQ5; /* BS5 -> index 4 */
    evt.btn_pressed = true;
    res = ui_shortcut_map_process(&evt, &ctx);
    assert(res.action_count == 1U);
    assert(res.actions[0].type == UI_SHORTCUT_ACTION_TRACK_SELECT);
    assert(res.actions[0].data.track.index == (UI_BTN_SEQ5 - UI_BTN_SEQ1));
    assert(ctx.track.active == true);

    /* Exit via SHIFT+BS11 while mode active. */
    g_shift_pressed = true;
    memset(&evt, 0, sizeof(evt));
    evt.has_button = true;
    evt.btn_id = UI_BTN_SEQ11;
    evt.btn_pressed = true;
    res = ui_shortcut_map_process(&evt, &ctx);
    assert(res.action_count == 1U);
    assert(res.actions[0].type == UI_SHORTCUT_ACTION_EXIT_TRACK_MODE);
    assert(ctx.track.active == false);
}

int main(void)
{
    test_track_metadata_initialisation();
    test_track_select_focus_updates();
    test_track_mode_mapping();
    printf("ui_mode_transition_tests: OK\n");
    return 0;
}
