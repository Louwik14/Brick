#include "ui/ui_led_backend.h"

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
void ui_led_backend_set_cart_track_count(uint8_t cart_idx, uint8_t tracks)
{
    (void)cart_idx; (void)tracks;
}
void ui_led_backend_set_track_focus(uint8_t track_index) { (void)track_index; }
void ui_led_backend_set_track_present(uint8_t track_index, bool present)
{
    (void)track_index; (void)present;
}
