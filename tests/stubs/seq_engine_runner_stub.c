#include "apps/seq_engine_runner.h"

void seq_engine_runner_init(void) {}
void seq_engine_runner_on_transport_play(void) {}
void seq_engine_runner_on_transport_stop(void) {}
void seq_engine_runner_on_clock_step(const clock_step_info_t *info) { (void)info; }
void seq_runner_set_active_pattern(uint8_t bank, uint8_t pattern) {
    (void)bank;
    (void)pattern;
}
