#include <stdio.h>
#include "tests/support/rt_blackbox.h"

#ifndef BB_CAP
#define BB_CAP 64u
#endif

static bb_ev_t   g_ring[BB_CAP];
static unsigned  g_head = 0, g_size = 0;
static unsigned  g_silent_ticks = 0;
static unsigned  g_events_this_tick = 0;
static unsigned  g_on[16];
static unsigned  g_off[16];
static uint32_t  g_in_flight[16][128];
static unsigned  g_unmatched_off = 0;
static unsigned  g_double_on = 0;
static uint32_t  g_max_len = 0;

void bb_reset(void){
  g_head=0;
  g_size=0;
  g_silent_ticks=0;
  g_events_this_tick=0;
  bb_track_counters_reset();
  bb_pair_reset();
}
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

void bb_track_counters_reset(void){
  for (int i=0;i<16;i++){
    g_on[i]=0;
    g_off[i]=0;
  }
}

void bb_track_on(uint8_t tr){ if (tr<16) g_on[tr]++; }
void bb_track_off(uint8_t tr){ if (tr<16) g_off[tr]++; }
unsigned bb_track_on_count(uint8_t tr){ return (tr<16)?g_on[tr]:0; }
unsigned bb_track_off_count(uint8_t tr){ return (tr<16)?g_off[tr]:0; }

void bb_pair_reset(void){
  g_unmatched_off = 0;
  g_double_on = 0;
  g_max_len = 0;
  for (unsigned tr = 0; tr < 16; ++tr){
    for (unsigned note = 0; note < 128; ++note){
      g_in_flight[tr][note] = 0;
    }
  }
}

void bb_pair_on(uint8_t track, uint8_t note, uint32_t tick){
  if (track >= 16 || note >= 128){
    return;
  }
  uint32_t *slot = &g_in_flight[track][note];
  if (*slot != 0){
    g_double_on++;
    return;
  }
  *slot = tick + 1U;
}

void bb_pair_off(uint8_t track, uint8_t note, uint32_t tick){
  if (track >= 16 || note >= 128){
    return;
  }
  uint32_t *slot = &g_in_flight[track][note];
  if (*slot == 0){
    g_unmatched_off++;
    return;
  }
  uint32_t tick_on = *slot - 1U;
  uint32_t len = (tick >= tick_on) ? (tick - tick_on) : 0U;
  if (len > g_max_len){
    g_max_len = len;
  }
  *slot = 0;
}

unsigned bb_unmatched_on(void){
  unsigned total = g_double_on;
  for (unsigned tr = 0; tr < 16; ++tr){
    for (unsigned note = 0; note < 128; ++note){
      if (g_in_flight[tr][note] != 0){
        total++;
      }
    }
  }
  return total;
}

unsigned bb_unmatched_off(void){
  return g_unmatched_off;
}

uint32_t bb_max_note_len_ticks(void){
  return g_max_len;
}
