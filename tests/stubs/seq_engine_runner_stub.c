#include "core/seq/seq_model.h"
#include "core/clock_manager.h"

void seq_engine_runner_attach_track(seq_model_track_t *track) {
    (void)track;
}

void seq_engine_runner_init(seq_model_track_t *pattern) {
    (void)pattern;
}

void seq_engine_runner_on_transport_play(void) {}
void seq_engine_runner_on_transport_stop(void) {}
void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
    (void)info;
}
