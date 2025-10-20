#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ui/ui_controller.h"
#include "ui/ui_overlay.h"
#include "ui/ui_seq_ui.h"
#include "ui/ui_arp_ui.h"
#include "apps/ui_arp_menu.h"
#include "cart/cart_bus.h"
#include "cart/cart_registry.h"
#include "core/cart_link.h"
#include "clock_manager.h"
#include "midi/midi.h"
#include "apps/ui_keyboard_app.h"
#include "apps/ui_keyboard_bridge.h"
#include "apps/kbd_input_mapper.h"
#include "apps/seq_recorder.h"
#include "core/seq/seq_model.h"
#include "ui/ui_model.h"

/* -------------------------------------------------------------------------- */
/* UI controller stubs                                                        */
/* -------------------------------------------------------------------------- */
static bool g_dirty_flag = false;

static const ui_cart_spec_t g_null_cart = {
    .cart_name   = "TEST",
    .overlay_tag = "SEQ",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t seq_ui_spec = {
    .cart_name   = "SEQ",
    .overlay_tag = "SEQ",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t seq_setup_ui_spec = {
    .cart_name   = "SEQ SETUP",
    .overlay_tag = "SETUP",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t arp_ui_spec = {
    .cart_name   = "ARP",
    .overlay_tag = "ARP",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t arp_setup_ui_spec = {
    .cart_name   = "ARP SETUP",
    .overlay_tag = "ARP",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t ui_keyboard_spec = {
    .cart_name   = "KBD",
    .overlay_tag = "KBD",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

const ui_cart_spec_t ui_keyboard_arp_menu_spec = {
    .cart_name   = "KBD ARP",
    .overlay_tag = "ARP",
    .menus       = { { 0 } },
    .cycles      = { { 0 } }
};

void ui_mark_dirty(void) { g_dirty_flag = true; }
bool ui_is_dirty(void) { return g_dirty_flag; }
void ui_clear_dirty(void) { g_dirty_flag = false; }

void ui_switch_cart(const ui_cart_spec_t *spec)
{
    if (spec == NULL) {
        spec = &g_null_cart;
    }
    ui_model_switch_cart(spec);
}

const ui_cart_spec_t *ui_get_cart(void)
{
    const ui_cart_spec_t *spec = ui_model_get_active_spec();
    return (spec != NULL) ? spec : &g_null_cart;
}

const ui_state_t *ui_get_state(void) { return ui_model_get_state(); }
const ui_menu_spec_t *ui_resolve_menu(uint8_t bm_index)
{
    (void)bm_index;
    const ui_cart_spec_t *spec = ui_get_cart();
    return (spec != NULL) ? &spec->menus[0] : &g_null_cart.menus[0];
}

void ui_on_button_menu(int index) { (void)index; }
void ui_on_button_page(int index) { (void)index; }
void ui_on_encoder(int enc_index, int delta) { (void)enc_index; (void)delta; }

/* -------------------------------------------------------------------------- */
/* Cart link / registry                                                       */
/* -------------------------------------------------------------------------- */
void cart_link_param_changed(uint16_t param_id, uint8_t value, bool is_bitwise, uint8_t bit_mask)
{
    (void)param_id;
    (void)value;
    (void)is_bitwise;
    (void)bit_mask;
}

uint8_t cart_link_shadow_get(cart_id_t id, uint16_t param)
{
    (void)id; (void)param;
    return 0U;
}

void cart_link_shadow_set(cart_id_t id, uint16_t param, uint8_t val)
{
    (void)id; (void)param; (void)val;
}

bool cart_registry_is_present(cart_id_t id)
{
    (void)id;
    return false;
}

const ui_cart_spec_t *cart_registry_switch(cart_id_t id)
{
    (void)id;
    return ui_get_cart();
}

cart_id_t cart_registry_get_active_id(void)
{
    return 0;
}

uint32_t cart_registry_get_uid(cart_id_t id)
{
    (void)id;
    return 0U;
}

bool cart_registry_find_by_uid(uint32_t uid, cart_id_t *out_id)
{
    (void)uid;
    if (out_id != NULL) {
        *out_id = 0;
    }
    return false;
}

/* -------------------------------------------------------------------------- */
/* Clock manager & MIDI                                                       */
/* -------------------------------------------------------------------------- */
void clock_manager_start(void) {}
void clock_manager_stop(void) {}

void midi_note_on(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t velocity)
{
    (void)dest; (void)ch; (void)note; (void)velocity;
}

void midi_note_off(midi_dest_t dest, uint8_t ch, uint8_t note, uint8_t velocity)
{
    (void)dest; (void)ch; (void)note; (void)velocity;
}

void midi_cc(midi_dest_t dest, uint8_t ch, uint8_t cc, uint8_t value)
{
    (void)dest; (void)ch; (void)cc; (void)value;
}

/* -------------------------------------------------------------------------- */
/* Keyboard bridge / app                                                      */
/* -------------------------------------------------------------------------- */
int8_t ui_keyboard_app_get_octave_shift(void)
{
    return 0;
}

void ui_keyboard_app_set_octave_shift(int8_t shift)
{
    (void)shift;
}

void ui_keyboard_bridge_on_transport_stop(void) {}

void kbd_input_mapper_process(uint8_t seq_index, bool pressed)
{
    (void)seq_index; (void)pressed;
}

/* -------------------------------------------------------------------------- */
/* Recorder                                                                   */
/* -------------------------------------------------------------------------- */
void seq_recorder_set_recording(bool recording)
{
    (void)recording;
}

void seq_recorder_attach_track(seq_model_track_t *track)
{
    (void)track;
}

/* -------------------------------------------------------------------------- */
/* ChibiOS hooks (minimal)                                                    */
/* -------------------------------------------------------------------------- */
systime_t chVTGetSystemTimeX(void) { return 0; }
systime_t chVTGetSystemTime(void) { return 0; }
void chThdSleepMilliseconds(uint32_t ms) { (void)ms; }
void chThdSleepMicroseconds(uint32_t us) { (void)us; }
void chRegSetThreadName(const char *name) { (void)name; }
void chThdCreateStatic(void *wa, size_t size, int prio, void (*func)(void *), void *arg) {
    (void)wa;
    (void)size;
    (void)prio;
    (void)func;
    (void)arg;
}
int chsnprintf(char *buf, size_t size, const char *fmt, ...) {
    (void)buf;
    (void)size;
    (void)fmt;
    return 0;
}
void chSysLock(void) {}
void chSysUnlock(void) {}
void chSysLockFromISR(void) {}
void chSysUnlockFromISR(void) {}
