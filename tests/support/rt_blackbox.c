#include <stdio.h>
#include "tests/support/rt_blackbox.h"

#ifndef BB_CAP
#define BB_CAP 64u
#endif

static bb_ev_t   g_ring[BB_CAP];
static unsigned  g_head = 0, g_size = 0;
static unsigned  g_silent_ticks = 0;
static unsigned  g_events_this_tick = 0;

void bb_reset(void){ g_head=0; g_size=0; g_silent_ticks=0; g_events_this_tick=0; }
static void _push(bb_ev_t ev){
  g_ring[(g_head + g_size) % BB_CAP] = ev;
  if (g_size < BB_CAP) g_size++; else g_head = (g_head + 1) % BB_CAP;
}
void bb_tick_begin(uint32_t tick){ (void)tick; g_events_this_tick = 0; }
void bb_tick_end(void){ if (g_events_this_tick == 0) g_silent_ticks++; }
void bb_log(uint32_t tick, uint8_t track, uint8_t step, uint8_t type){
  _push((bb_ev_t){tick,track,step,type}); g_events_this_tick++;
}
unsigned bb_silent_ticks(void){ return g_silent_ticks; }
unsigned bb_count(void){ return g_size; }
void bb_dump(void){
  printf("Blackbox last %u events:\n", g_size);
  for (unsigned i=0;i<g_size;i++){
    unsigned idx = (g_head + i) % BB_CAP;
    const bb_ev_t *e = &g_ring[idx];
    printf("  t=%lu tr=%u st=%u ty=%u\n",
           (unsigned long)e->tick, e->track, e->step, e->type);
  }
}
