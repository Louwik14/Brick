#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "ui/ui_overlay.h"

static bool s_overlay_active = false;
static const ui_cart_spec_t *s_overlay_spec = NULL;
static const ui_cart_spec_t *s_overlay_host = NULL;
static ui_custom_mode_t s_custom_mode = UI_CUSTOM_NONE;
static char s_banner_tag[32];
static const char *s_banner_cart = NULL;

void ui_overlay_enter(ui_overlay_id_t id, const ui_cart_spec_t *spec)
{
    (void)id;
    s_overlay_active = true;
    s_overlay_spec = spec;
}

void ui_overlay_exit(void)
{
    s_overlay_active = false;
    s_overlay_spec = NULL;
}

bool ui_overlay_is_active(void)
{
    return s_overlay_active;
}

void ui_overlay_switch_subspec(const ui_cart_spec_t *spec)
{
    s_overlay_spec = spec;
}

const ui_cart_spec_t *ui_overlay_get_spec(void)
{
    return s_overlay_spec;
}

void ui_overlay_set_custom_mode(ui_custom_mode_t mode)
{
    s_custom_mode = mode;
}

ui_custom_mode_t ui_overlay_get_custom_mode(void)
{
    return s_custom_mode;
}

void ui_overlay_prepare_banner(const ui_cart_spec_t *src_mode,
                               const ui_cart_spec_t *src_setup,
                               const ui_cart_spec_t **dst_mode,
                               const ui_cart_spec_t **dst_setup,
                               const ui_cart_spec_t *prev_cart,
                               const char *mode_tag)
{
    if (dst_mode) {
        *dst_mode = src_mode;
    }
    if (dst_setup) {
        *dst_setup = src_setup;
    }
    (void)prev_cart;
    ui_overlay_update_banner_tag(mode_tag);
}

void ui_overlay_set_banner_override(const char *cart_name, const char *tag)
{
    s_banner_cart = cart_name;
    ui_overlay_update_banner_tag(tag);
}

void ui_overlay_update_banner_tag(const char *tag)
{
    const char *src = (tag && tag[0] != '\0') ? tag : "";
    (void)snprintf(s_banner_tag, sizeof(s_banner_tag), "%s", src);
    s_banner_tag[sizeof(s_banner_tag) - 1U] = '\0';
}

const char *ui_overlay_get_banner_cart_override(void)
{
    return s_overlay_active ? s_banner_cart : NULL;
}

const char *ui_overlay_get_banner_tag_override(void)
{
    return s_overlay_active ? s_banner_tag : NULL;
}

const ui_cart_spec_t *ui_overlay_get_host_cart(void)
{
    return s_overlay_host;
}
