#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "ui/ui_mute_backend.h"

static bool g_stub_muted[16];
static bool g_stub_prepared[16];

bool g_stub_mute_clear_called = false;

void ui_mute_backend_init(void)
{
    memset(g_stub_muted, 0, sizeof(g_stub_muted));
    memset(g_stub_prepared, 0, sizeof(g_stub_prepared));
    g_stub_mute_clear_called = false;
}

void ui_mute_backend_apply(uint8_t track, bool mute)
{
    if (track < 16U) {
        g_stub_muted[track] = mute;
    }
}

void ui_mute_backend_toggle(uint8_t track)
{
    if (track < 16U) {
        g_stub_muted[track] = !g_stub_muted[track];
    }
}

void ui_mute_backend_toggle_prepare(uint8_t track)
{
    if (track < 16U) {
        g_stub_prepared[track] = !g_stub_prepared[track];
    }
}

void ui_mute_backend_commit(void)
{
    for (uint8_t i = 0; i < 16U; ++i) {
        if (g_stub_prepared[i]) {
            g_stub_muted[i] = !g_stub_muted[i];
            g_stub_prepared[i] = false;
        }
    }
}

void ui_mute_backend_cancel(void)
{
    memset(g_stub_prepared, 0, sizeof(g_stub_prepared));
}

void ui_mute_backend_publish_state(void)
{
    /* no-op for host tests */
}

void ui_mute_backend_clear(void)
{
    memset(g_stub_prepared, 0, sizeof(g_stub_prepared));
    g_stub_mute_clear_called = true;
}

bool ui_mute_backend_is_muted(uint8_t track)
{
    return (track < 16U) ? g_stub_muted[track] : false;
}

bool ui_mute_backend_is_prepared(uint8_t track)
{
    return (track < 16U) ? g_stub_prepared[track] : false;
}
