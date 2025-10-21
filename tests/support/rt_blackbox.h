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

#ifdef __cplusplus
}
#endif
