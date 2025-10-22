#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tick;
    uint8_t ch;
    uint8_t note;
    uint8_t vel;
    uint8_t ty; /* 1=ON, 2=OFF, 3=CC123 */
} midi_probe_ev_t;

void midi_probe_reset(void);
void midi_probe_tick_begin(uint32_t tick);
void midi_probe_log(uint32_t tick, uint8_t ch, uint8_t note, uint8_t vel, uint8_t ty);
unsigned midi_probe_count(void);
unsigned midi_probe_silent_ticks(void);
const midi_probe_ev_t *midi_probe_snapshot(unsigned *out_count);
void midi_probe_tick_end(void);

#ifdef __cplusplus
}
#endif
