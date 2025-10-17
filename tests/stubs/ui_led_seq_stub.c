#include "ui/ui_led_seq.h"

void ui_led_seq_update_from_app(const seq_runtime_t *rt) { (void)rt; }
void ui_led_seq_on_clock_tick(uint8_t step_index) { (void)step_index; }
void ui_led_seq_set_running(bool running) { (void)running; }
void ui_led_seq_set_total_span(uint16_t total_steps) { (void)total_steps; }
void ui_led_seq_render(void) {}
