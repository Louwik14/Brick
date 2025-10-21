#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint32_t tick; uint8_t track, step, type; } bb_ev_t; // type: 1=ON, 2=OFF
void bb_reset(void);
void bb_tick_begin(uint32_t tick);
void bb_tick_end(void);
void bb_log(uint32_t tick, uint8_t track, uint8_t step, uint8_t type);
unsigned bb_silent_ticks(void);
unsigned bb_count(void);
void bb_dump(void);

void bb_track_counters_reset(void);
void bb_track_on(uint8_t track);
void bb_track_off(uint8_t track);
unsigned bb_track_on_count(uint8_t track);
unsigned bb_track_off_count(uint8_t track);

// Pairing & invariants
void bb_pair_reset(void);
void bb_pair_on(uint8_t track, uint8_t note, uint32_t tick);
void bb_pair_off(uint8_t track, uint8_t note, uint32_t tick);

// Stats invariants
unsigned bb_unmatched_on(void);
unsigned bb_unmatched_off(void);
uint32_t bb_max_note_len_ticks(void);

#ifdef __cplusplus
}
#endif
