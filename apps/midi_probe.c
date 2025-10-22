#include "apps/midi_probe.h"

#include <stddef.h>

#ifndef MIDI_PROBE_CAP
#define MIDI_PROBE_CAP 128u
#endif

static midi_probe_ev_t g_ring[MIDI_PROBE_CAP];
static unsigned g_head = 0U;
static unsigned g_size = 0U;
static unsigned g_silent = 0U;
static unsigned g_tick_events = 0U;

static void _push(midi_probe_ev_t ev) {
    const unsigned tail = (g_head + g_size) % MIDI_PROBE_CAP;
    g_ring[tail] = ev;
    if (g_size < MIDI_PROBE_CAP) {
        ++g_size;
    } else {
        g_head = (g_head + 1U) % MIDI_PROBE_CAP;
    }
}

void midi_probe_reset(void) {
    g_head = 0U;
    g_size = 0U;
    g_silent = 0U;
    g_tick_events = 0U;
}

void midi_probe_tick_begin(uint32_t tick) {
    (void)tick;
    g_tick_events = 0U;
}

void midi_probe_log(uint32_t tick, uint8_t ch, uint8_t note, uint8_t vel, uint8_t ty) {
    _push((midi_probe_ev_t){tick, ch, note, vel, ty});
    ++g_tick_events;
}

unsigned midi_probe_count(void) {
    return g_size;
}

unsigned midi_probe_silent_ticks(void) {
    return g_silent;
}

const midi_probe_ev_t *midi_probe_snapshot(unsigned *out_count) {
    if (out_count != NULL) {
        *out_count = g_size;
    }
    return g_ring;
}

__attribute__((weak)) void midi_probe_tick_end(void) {
    if (g_tick_events == 0U) {
        ++g_silent;
    }
}
