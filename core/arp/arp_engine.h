#ifndef BRICK_CORE_ARP_ARP_ENGINE_H
#define BRICK_CORE_ARP_ARP_ENGINE_H

// --- ARP: moteur d'arpégiateur configurables ---

#include <stdbool.h>
#include <stdint.h>
#include "ch.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- ARP: Cadence principale (note rate) ---
typedef enum {
  ARP_RATE_QUARTER = 0,
  ARP_RATE_EIGHTH,
  ARP_RATE_SIXTEENTH,
  ARP_RATE_THIRTY_SECOND,
  ARP_RATE_QUARTER_TRIPLET,
  ARP_RATE_EIGHTH_TRIPLET,
  ARP_RATE_SIXTEENTH_TRIPLET,
  ARP_RATE_THIRTY_SECOND_TRIPLET,
  ARP_RATE_COUNT
} arp_rate_t;

// --- ARP: Modes de direction/pattern ---
typedef enum {
  ARP_PATTERN_UP = 0,
  ARP_PATTERN_DOWN,
  ARP_PATTERN_UP_DOWN,
  ARP_PATTERN_RANDOM,
  ARP_PATTERN_CHORD,
  ARP_PATTERN_COUNT
} arp_pattern_t;

// --- ARP: Modes d'accentuation ---
typedef enum {
  ARP_ACCENT_OFF = 0,
  ARP_ACCENT_FIRST,
  ARP_ACCENT_ALTERNATE,
  ARP_ACCENT_RANDOM,
  ARP_ACCENT_COUNT
} arp_accent_t;

// --- ARP: Modes de strum ---
typedef enum {
  ARP_STRUM_OFF = 0,
  ARP_STRUM_UP,
  ARP_STRUM_DOWN,
  ARP_STRUM_ALT,
  ARP_STRUM_RANDOM,
  ARP_STRUM_COUNT
} arp_strum_t;

// --- ARP: Mode de synchronisation ---
typedef enum {
  ARP_SYNC_INTERNAL = 0,
  ARP_SYNC_MIDI_CLOCK,
  ARP_SYNC_FREERUN,
  ARP_SYNC_COUNT
} arp_sync_mode_t;

// --- ARP: Configuration complète ---
typedef struct {
  bool                enabled;
  bool                hold_enabled;       // --- ARP FIX: Hold (On/Off) ---
  arp_rate_t          rate;
  uint8_t             octave_range;      // 1..4
  arp_pattern_t       pattern;
  uint8_t             gate_percent;      // 10..100
  uint8_t             swing_percent;     // 0..75
  arp_accent_t        accent;
  uint8_t             vel_accent;        // --- ARP FIX: intensité accent 0..127 ---
  arp_strum_t         strum_mode;
  uint8_t             strum_offset_ms;   // --- ARP FIX: 0..60 ms ---
  uint8_t             repeat_count;      // 1..4
  int8_t              transpose;         // ±12
  uint8_t             spread_percent;    // 0..100
  int8_t              octave_shift;      // ±1
  uint8_t             direction_behavior;// 0..2 (Normal/PingPong/RandomWalk)
  arp_sync_mode_t     sync_mode;
} arp_config_t;

// --- ARP: Callbacks NoteOn/NoteOff ---
typedef struct {
  void (*note_on)(uint8_t note, uint8_t velocity, systime_t when); // --- ARP FIX: timestamp pour note on ---
  void (*note_off)(uint8_t note);
} arp_callbacks_t;

// --- ARP: Moteur runtime ---
typedef struct {
  arp_config_t   config;
  arp_callbacks_t callbacks;

  uint8_t        phys_notes[32];
  uint8_t        phys_velocities[32];
  uint8_t        phys_count;

  uint8_t        latched_notes[32];
  uint8_t        latched_velocities[32];
  uint8_t        latched_count;
  bool           latched_active;

  uint8_t        pattern_notes[32];
  uint8_t        pattern_velocities[32];
  uint8_t        pattern_count;

  systime_t      next_event;
  systime_t      base_period;
  systime_t      swing_period;
  systime_t      strum_offset;

  uint32_t       step_index;
  uint8_t        repeat_index;
  uint8_t        direction;        // 0 up,1 down
  bool           running;
  uint8_t        strum_phase;      // --- ARP FIX: alt/rnd strum mémoire ---

  uint8_t        active_notes[64];
  systime_t      active_until[64];
  uint8_t        active_count;

  uint8_t        pending_on_notes[64];
  uint8_t        pending_on_vel[64];
  systime_t      pending_on_time[64];
  uint8_t        pending_on_count;

  uint32_t       random_seed;
} arp_engine_t;

// --- ARP: API principale ---
void arp_init(arp_engine_t *engine, const arp_config_t *cfg);
void arp_set_callbacks(arp_engine_t *engine, const arp_callbacks_t *cb);
void arp_set_config(arp_engine_t *engine, const arp_config_t *cfg);
void arp_note_input(arp_engine_t *engine, uint8_t note, uint8_t velocity, bool pressed);
void arp_tick(arp_engine_t *engine, systime_t now);
void arp_stop_all(arp_engine_t *engine);
void arp_set_hold(arp_engine_t *engine, bool enabled); // --- ARP FIX: API dédiée Hold ---

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CORE_ARP_ARP_ENGINE_H */
