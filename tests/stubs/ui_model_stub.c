#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ui/ui_model.h"

static char g_last_tag[32] = "SEQ";
static ui_state_t g_model_state;
static const ui_cart_spec_t *g_active_spec;

static const ui_cart_spec_t g_stub_cart_spec = {
    .cart_name   = "SEQ",
    .overlay_tag = "SEQ",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

static const ui_cart_spec_t *fallback_spec(void)
{
    return &g_stub_cart_spec;
}

static void assign_spec(const ui_cart_spec_t *spec)
{
    if (spec == NULL) {
        spec = fallback_spec();
    }
    g_active_spec = spec;
    g_model_state.spec = spec;
    g_model_state.cur_menu = 0U;
    g_model_state.cur_page = 0U;
    g_model_state.shift = false;
}

void ui_model_init(const ui_cart_spec_t *initial_spec)
{
    assign_spec(initial_spec);
}

void ui_model_switch_cart(const ui_cart_spec_t *spec)
{
    assign_spec(spec);
}

void ui_model_restore_last_cart(void)
{
    assign_spec(g_active_spec);
}

ui_state_t *ui_model_get_state(void)
{
    assign_spec(g_active_spec);
    return &g_model_state;
}

const ui_cart_spec_t *ui_model_get_active_spec(void)
{
    return (g_active_spec != NULL) ? g_active_spec : fallback_spec();
}

const char *ui_model_get_active_overlay_tag(void)
{
    return g_last_tag;
}

void ui_model_set_active_overlay_tag(const char *tag)
{
    const char *src = (tag && tag[0] != '\0') ? tag : "SEQ";
    (void)snprintf(g_last_tag, sizeof(g_last_tag), "%s", src);
    g_last_tag[sizeof(g_last_tag) - 1U] = '\0';
}
