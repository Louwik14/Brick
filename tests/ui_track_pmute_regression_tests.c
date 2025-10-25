#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_backend.h"
#include "ui/ui_input.h"
#include "ui/ui_led_backend.h"
#include "ui/ui_mute_backend.h"
#include "apps/seq_led_bridge.h"
#include "core/seq/seq_access.h"
#include "tests/runtime_compat.h"
#include "ui/ui_led_palette.h"

static bool g_shift_pressed;

bool ui_input_shift_is_pressed(void)
{
    return g_shift_pressed;
}

static void set_shift(bool pressed)
{
    g_shift_pressed = pressed;
}

static void run_event(uint8_t btn, bool pressed)
{
    ui_input_event_t evt;
    memset(&evt, 0, sizeof(evt));
    evt.has_button = true;
    evt.btn_id = btn;
    evt.btn_pressed = pressed;
    ui_backend_process_input(&evt);
}

static void setup_runtime(void)
{
    g_shift_pressed = false;
    ui_led_backend_init();
    ui_mute_backend_init();
    seq_runtime_init();
    seq_led_bridge_init();
    seq_project_t *project = seq_runtime_compat_access_project_mut();
    if (project != NULL) {
        uint8_t active_bank = seq_project_get_active_bank(project);
        uint8_t active_pattern = seq_project_get_active_pattern_index(project);
        seq_led_bridge_set_active(active_bank, active_pattern);
    } else {
        seq_led_bridge_set_active(0U, 0U);
    }
    seq_led_bridge_bind_project(project);
    ui_backend_init_runtime();
    seq_led_bridge_publish();
    ui_led_backend_refresh();
}

static void seed_pattern(void)
{
    seq_led_bridge_step_clear(0U);
    seq_led_bridge_step_set_voice(0U, 0U, 60U, 100U);
    seq_led_bridge_publish();
    ui_led_backend_refresh();
}

static void test_track_overlay_placeholder(void)
{
    setup_runtime();

    set_shift(true);
    run_event(UI_BTN_SEQ11, true);
    set_shift(false);

    const ui_mode_context_t *ctx = ui_backend_get_mode_context();
    assert(ctx != NULL);
    assert(ctx->track.active == true);
    assert(strcmp(ui_backend_get_mode_label(), "TRACK") == 0);
    assert(ui_led_backend_debug_get_mode() == UI_LED_MODE_TRACK);
    assert(ui_led_backend_debug_queue_drops() == 0U);

    set_shift(true);
    run_event(UI_BTN_SEQ11, true);
    set_shift(false);

    ctx = ui_backend_get_mode_context();
    assert(ctx->track.active == false);
    assert(strcmp(ui_backend_get_mode_label(), "SEQ") == 0);
    assert(ui_led_backend_debug_get_mode() == UI_LED_MODE_SEQ);
}

static void test_mute_led_state(void)
{
    setup_runtime();
    seed_pattern();

    set_shift(true);
    run_event(UI_BTN_PLUS, true); /* Enter QUICK mute */
    set_shift(false);
    assert(ui_led_backend_debug_get_mode() == UI_LED_MODE_MUTE);

    run_event(UI_BTN_SEQ1, true); /* Toggle track 0 */
    ui_led_backend_refresh();
    assert(ui_mute_backend_is_muted(0U) == true);
    assert(ui_led_backend_debug_track_muted(0U) == true);

    run_event(UI_BTN_PLUS, false); /* Exit QUICK mute */
    ui_led_backend_refresh();
    assert(ui_led_backend_debug_get_mode() == UI_LED_MODE_SEQ);
    assert(ui_led_backend_debug_queue_drops() == 0U);

    const led_state_t *leds = ui_led_backend_debug_led_state();
    assert(leds != NULL);
    const led_state_t seq1 = leds[LED_SEQ1];
    assert(!(seq1.color.r == UI_LED_COL_MUTE_RED.r &&
             seq1.color.g == UI_LED_COL_MUTE_RED.g &&
             seq1.color.b == UI_LED_COL_MUTE_RED.b));

    set_shift(true);
    run_event(UI_BTN_PLUS, true); /* Re-enter QUICK mute */
    set_shift(false);
    ui_led_backend_refresh();
    assert(ui_led_backend_debug_get_mode() == UI_LED_MODE_MUTE);
    assert(ui_led_backend_debug_track_muted(0U) == true);

    leds = ui_led_backend_debug_led_state();
    const led_state_t seq1_mute = leds[LED_SEQ1];
    assert(seq1_mute.color.r == UI_LED_COL_MUTE_RED.r &&
           seq1_mute.color.g == UI_LED_COL_MUTE_RED.g &&
           seq1_mute.color.b == UI_LED_COL_MUTE_RED.b);
    assert(ui_led_backend_debug_queue_drops() == 0U);
}

int main(void)
{
    test_track_overlay_placeholder();
    test_mute_led_state();
    printf("ui_track_pmute_regression_tests: OK\n");
    return 0;
}
