#include "arp_engine.h"

// --- ARP: runtime du moteur ---

#include <string.h>
#include "clock_manager.h"

// --- ARP: Helpers internes ---

#define ARP_ARRAY_SIZE(a) ((uint8_t)(sizeof(a)/sizeof((a)[0])))

static inline uint32_t _lcg_next(arp_engine_t *engine) {
  engine->random_seed = (engine->random_seed * 1664525u) + 1013904223u;
  return engine->random_seed;
}

static inline uint8_t _clamp_u7(int32_t value) {
  if (value < 0) return 0u;
  if (value > 127) return 127u;
  return (uint8_t)value;
}

static inline uint8_t _clamp_note(int32_t value) {
  if (value < 0) return 0u;
  if (value > 127) return 127u;
  return (uint8_t)value;
}

static inline void _sanitise_config(arp_config_t *cfg) {
  if (!cfg) return;
  cfg->hold_enabled = cfg->hold_enabled ? true : false; // --- ARP FIX: normaliser Hold ---
  if (cfg->octave_range == 0u) cfg->octave_range = 1u;
  if (cfg->octave_range > 4u) cfg->octave_range = 4u;
  if (cfg->gate_percent < 10u) cfg->gate_percent = 10u;
  if (cfg->gate_percent > 100u) cfg->gate_percent = 100u;
  if (cfg->swing_percent > 75u) cfg->swing_percent = 75u;
  if (cfg->velocity_random > 20u) cfg->velocity_random = 20u;
  if (cfg->repeat_count == 0u) cfg->repeat_count = 1u;
  if (cfg->repeat_count > 4u) cfg->repeat_count = 4u;
  if (cfg->transpose < -12) cfg->transpose = -12;
  if (cfg->transpose > 12) cfg->transpose = 12;
  if (cfg->octave_shift < -1) cfg->octave_shift = -1;
  if (cfg->octave_shift > 1) cfg->octave_shift = 1;
  if (cfg->pattern_select == 0u) cfg->pattern_select = 1u;
  if (cfg->pattern_select > 8u) cfg->pattern_select = 8u;
  if (cfg->pattern_morph > 100u) cfg->pattern_morph = 100u;
  if (cfg->spread_percent > 100u) cfg->spread_percent = 100u;
  if (cfg->lfo_depth > 127u) cfg->lfo_depth = 127u;
  if (cfg->lfo_rate > 127u) cfg->lfo_rate = 127u;
  if (cfg->rate >= ARP_RATE_COUNT) cfg->rate = ARP_RATE_SIXTEENTH;
  if (cfg->pattern >= ARP_PATTERN_COUNT) cfg->pattern = ARP_PATTERN_UP;
  if (cfg->accent >= ARP_ACCENT_COUNT) cfg->accent = ARP_ACCENT_OFF;
  if (cfg->strum_mode >= ARP_STRUM_COUNT) cfg->strum_mode = ARP_STRUM_OFF;
  if (cfg->trigger_mode >= ARP_TRIGGER_COUNT) cfg->trigger_mode = ARP_TRIGGER_HOLD;
  if (cfg->direction_behavior > 2u) cfg->direction_behavior = 0u;
  if (cfg->lfo_target >= ARP_LFO_TARGET_COUNT) cfg->lfo_target = ARP_LFO_TARGET_GATE;
  if (cfg->sync_mode >= ARP_SYNC_COUNT) cfg->sync_mode = ARP_SYNC_INTERNAL;
}

static inline systime_t _seconds_to_ticks(float seconds) {
  if (seconds <= 0.0005f) seconds = 0.0005f;
  const uint32_t usec = (uint32_t)((seconds * 1000000.0f) + 0.5f);
  return chTimeUS2I(usec);
}

static systime_t _compute_period(const arp_config_t *cfg, float bpm) {
  if (!cfg || bpm <= 0.0f) {
    bpm = 120.0f;
  }
  const float quarter = 60.0f / bpm;
  float duration = quarter;
  switch (cfg->rate) {
    case ARP_RATE_QUARTER: duration = quarter; break;
    case ARP_RATE_EIGHTH: duration = quarter * 0.5f; break;
    case ARP_RATE_SIXTEENTH: duration = quarter * 0.25f; break;
    case ARP_RATE_THIRTY_SECOND: duration = quarter * 0.125f; break;
    case ARP_RATE_QUARTER_TRIPLET: duration = quarter * (2.0f / 3.0f); break; // --- ARP FIX: triplet rates ---
    case ARP_RATE_EIGHTH_TRIPLET: duration = quarter / 3.0f; break;
    case ARP_RATE_SIXTEENTH_TRIPLET: duration = quarter / 6.0f; break;
    case ARP_RATE_THIRTY_SECOND_TRIPLET: duration = quarter / 12.0f; break;
    default: duration = quarter * 0.25f; break;
  }
  return _seconds_to_ticks(duration);
}

static void _recompute_periods(arp_engine_t *engine) {
  if (!engine) return;
  const float bpm = clock_manager_get_bpm();
  engine->base_period = _compute_period(&engine->config, bpm);
  if (engine->base_period < TIME_MS2I(1)) {
    engine->base_period = TIME_MS2I(1);
  }
  engine->swing_period = (engine->base_period * engine->config.swing_percent) / 100u;
  engine->strum_offset = TIME_MS2I(engine->config.strum_offset_ms);
}

static void _clear_active_notes(arp_engine_t *engine) {
  for (uint8_t i = 0; i < engine->active_count; ++i) {
    if (engine->callbacks.note_off) {
      engine->callbacks.note_off(engine->active_notes[i]);
    }
  }
  engine->active_count = 0u;
  engine->pending_on_count = 0u;
}

static void _reset_runtime(arp_engine_t *engine, systime_t now) {
  engine->step_index = 0u;
  engine->repeat_index = 0u;
  engine->direction = 0u;
  engine->next_event = now;
  engine->arp_notes_latched = (engine->pattern_count > 0u);
}

static void _latch_notes_from_held(arp_engine_t *engine) {
  engine->pattern_count = engine->held_count;
  for (uint8_t i = 0; i < engine->held_count; ++i) {
    engine->pattern_notes[i] = engine->held_notes[i];
    engine->pattern_velocities[i] = engine->held_velocities[i];
  }
  engine->arp_notes_latched = (engine->pattern_count > 0u);
}

static void _try_start(arp_engine_t *engine, systime_t now) {
  bool should_run = engine->config.enabled;
  if (!should_run) {
    engine->running = false;
    return;
  }

  if (engine->config.trigger_mode == ARP_TRIGGER_FREERUN) {
    if (engine->pattern_count == 0u && engine->held_count > 0u) {
      _latch_notes_from_held(engine);
    }
    should_run = (engine->pattern_count > 0u);
  } else {
    if (engine->held_count > 0u) {
      _latch_notes_from_held(engine);
      should_run = true;
    } else if (engine->config.hold_enabled && engine->arp_notes_latched && engine->pattern_count > 0u) {
      should_run = true; // --- ARP FIX: Hold garde l’arp actif après release ---
    } else {
      should_run = false;
    }
  }

  if (!should_run) {
    engine->running = false;
    return;
  }

  if (!engine->running) {
    _reset_runtime(engine, now);
    engine->running = true;
  }
}

static void _update_running_from_release(arp_engine_t *engine, systime_t now) {
  if (engine->config.trigger_mode == ARP_TRIGGER_FREERUN) {
    if (engine->pattern_count == 0u && engine->held_count == 0u) {
      engine->running = false;
      engine->arp_notes_latched = false;
    }
    return;
  }

  if (engine->held_count == 0u) {
    if (engine->config.hold_enabled && engine->pattern_count > 0u) {
      engine->arp_notes_latched = true; // --- ARP FIX: Hold maintient le dernier pattern ---
      return;
    }
    engine->running = false;
    engine->arp_notes_latched = false;
    engine->pattern_count = 0u;
    engine->next_event = now;
  }
}

static void _schedule_note_off(arp_engine_t *engine, uint8_t note, systime_t off_time) {
  if (engine->active_count >= ARP_ARRAY_SIZE(engine->active_notes)) {
    return;
  }
  engine->active_notes[engine->active_count] = note;
  engine->active_until[engine->active_count] = off_time;
  engine->active_count++;
}

static void _queue_note_on(arp_engine_t *engine, uint8_t note, uint8_t velocity, systime_t when) {
  if (engine->pending_on_count >= ARP_ARRAY_SIZE(engine->pending_on_notes)) {
    return;
  }
  engine->pending_on_notes[engine->pending_on_count] = note;
  engine->pending_on_vel[engine->pending_on_count] = velocity;
  engine->pending_on_time[engine->pending_on_count] = when;
  engine->pending_on_count++;
}

static void _dispatch_pending_note_ons(arp_engine_t *engine, systime_t now) {
  uint8_t w = 0u;
  for (uint8_t i = 0u; i < engine->pending_on_count; ++i) {
    if (engine->pending_on_time[i] <= now) {
      const systime_t event_time = engine->pending_on_time[i];
      if (engine->callbacks.note_on) {
        engine->callbacks.note_on(engine->pending_on_notes[i], engine->pending_on_vel[i], event_time);
      }
      const systime_t gate_len = (engine->base_period * engine->config.gate_percent) / 100u;
      systime_t off_time = event_time + gate_len;
      if (off_time <= event_time) {
        off_time = event_time + 1;
      }
      _schedule_note_off(engine, engine->pending_on_notes[i], off_time);
    } else {
      engine->pending_on_notes[w] = engine->pending_on_notes[i];
      engine->pending_on_vel[w] = engine->pending_on_vel[i];
      engine->pending_on_time[w] = engine->pending_on_time[i];
      ++w;
    }
  }
  engine->pending_on_count = w;
}

static void _dispatch_note_offs(arp_engine_t *engine, systime_t now) {
  uint8_t w = 0u;
  for (uint8_t i = 0u; i < engine->active_count; ++i) {
    if (engine->active_until[i] <= now) {
      if (engine->callbacks.note_off) {
        engine->callbacks.note_off(engine->active_notes[i]);
      }
    } else {
      engine->active_notes[w] = engine->active_notes[i];
      engine->active_until[w] = engine->active_until[i];
      ++w;
    }
  }
  engine->active_count = w;
}

static uint8_t _accent_velocity(const arp_engine_t *engine, uint8_t base, uint32_t step) {
  uint8_t vel = base;
  switch (engine->config.accent) {
    case ARP_ACCENT_OFF: break;
    case ARP_ACCENT_FIRST:
      if (step == 0u) {
        vel = _clamp_u7(base + 20);
      }
      break;
    case ARP_ACCENT_ALTERNATE:
      if ((step & 1u) != 0u) {
        vel = _clamp_u7(base + 12);
      }
      break;
    case ARP_ACCENT_RANDOM:
      vel = _clamp_u7(base + (int8_t)((int32_t)(_lcg_next((arp_engine_t *)engine) % 41u) - 20));
      break;
    default:
      break;
  }
  if (engine->config.velocity_random > 0u) {
    int32_t spread = (int32_t)(engine->config.velocity_random);
    int32_t delta = (int32_t)(_lcg_next((arp_engine_t *)engine) % (uint32_t)(spread * 2 + 1)) - (int32_t)spread;
    vel = _clamp_u7((int32_t)vel + delta);
  }
  return vel;
}

static void _apply_lfo(arp_engine_t *engine, uint8_t *note, uint8_t *velocity, systime_t now) {
  if (engine->config.lfo_depth == 0u) {
    return;
  }
  uint32_t phase = (uint32_t)(now / TIME_MS2I(10)) * (uint32_t)(engine->config.lfo_rate + 1u);
  int32_t modulation = (int32_t)((phase & 0xFFu) - 128);
  modulation = (modulation * engine->config.lfo_depth) / 128;
  switch (engine->config.lfo_target) {
    case ARP_LFO_TARGET_GATE:
      engine->swing_period = (engine->base_period * engine->config.swing_percent) / 100u;
      break;
    case ARP_LFO_TARGET_VELOCITY:
      *velocity = _clamp_u7((int32_t)(*velocity) + modulation / 4);
      break;
    case ARP_LFO_TARGET_PITCH:
      *note = _clamp_note((int32_t)(*note) + modulation / 8);
      break;
    default:
      break;
  }
}

static uint8_t _resolve_direction_index(arp_engine_t *engine, uint8_t count) {
  if (count == 0u) {
    return 0u;
  }
  switch (engine->config.pattern) {
    case ARP_PATTERN_UP:
      return (uint8_t)(engine->step_index % count);
    case ARP_PATTERN_DOWN:
      return (uint8_t)((count - 1u) - (engine->step_index % count));
    case ARP_PATTERN_RANDOM:
      return (uint8_t)(_lcg_next(engine) % count);
    case ARP_PATTERN_UP_DOWN: {
      if (count == 1u) return 0u;
      if (engine->direction == 0u) {
        uint8_t idx = (uint8_t)(engine->step_index % count);
        if (idx == (count - 1u)) {
          engine->direction = 1u;
        }
        return idx;
      } else {
        uint8_t idx = (uint8_t)(count - 1u - (engine->step_index % count));
        if (idx == 0u) {
          engine->direction = 0u;
        }
        return idx;
      }
    }
    case ARP_PATTERN_CHORD:
      return 0u;
    default:
      return (uint8_t)(engine->step_index % count);
  }
}

static void _advance_step(arp_engine_t *engine, uint8_t sequence_len) {
  engine->repeat_index++;
  if (engine->repeat_index >= engine->config.repeat_count) {
    engine->repeat_index = 0u;
    engine->step_index++;
    if (engine->config.direction_behavior == 1u && sequence_len > 1u) {
      if (engine->step_index % (sequence_len * 2u) == 0u) {
        engine->direction ^= 1u;
      }
    } else if (engine->config.direction_behavior == 2u && sequence_len > 0u) {
      engine->direction = (uint8_t)(_lcg_next(engine) & 0x1u);
    }
  }
}

static uint8_t _build_sequence(const arp_engine_t *engine, uint8_t *notes_out, uint8_t *vel_out) {
  const uint8_t base_count = engine->pattern_count;
  if (base_count == 0u) {
    return 0u;
  }
  uint8_t count = 0u;
  const int32_t transpose = (int32_t)engine->config.transpose + (int32_t)(engine->config.octave_shift * 12);
  for (uint8_t oct = 0u; oct < engine->config.octave_range; ++oct) {
    for (uint8_t i = 0u; i < base_count; ++i) {
      if (count >= 48u) break;
      int32_t note = (int32_t)engine->pattern_notes[i] + (int32_t)(oct * 12u) + transpose;
      const int32_t spread = (int32_t)((int32_t)engine->config.spread_percent * (int32_t)i) / 25;
      note += spread;
      notes_out[count] = _clamp_note(note);
      vel_out[count] = engine->pattern_velocities[i];
      ++count;
    }
  }
  if (count == 0u) {
    return 0u;
  }
  for (uint8_t i = 0u; i < count; ++i) {
    const uint8_t morph_target = (uint8_t)((engine->config.pattern_morph * i) / (count ? count : 1u));
    notes_out[i] = _clamp_note((int32_t)notes_out[i] + morph_target / 12);
  }
  if (engine->config.pattern_select > 1u && count > 1u) {
    uint8_t rotate = (uint8_t)((engine->config.pattern_select - 1u) % count);
    while (rotate-- > 0u) {
      uint8_t first_note = notes_out[0];
      uint8_t first_vel = vel_out[0];
      for (uint8_t j = 0u; j + 1u < count; ++j) {
        notes_out[j] = notes_out[j + 1u];
        vel_out[j] = vel_out[j + 1u];
      }
      notes_out[count - 1u] = first_note;
      vel_out[count - 1u] = first_vel;
    }
  }
  return count;
}

static void _emit_single_note(arp_engine_t *engine, uint8_t note, uint8_t velocity, systime_t now) {
  _apply_lfo(engine, &note, &velocity, now);
  const systime_t gate_len = (engine->base_period * engine->config.gate_percent) / 100u;
  systime_t off_time = now + gate_len;
  if (off_time <= now) {
    off_time = now + 1;
  }
  if (engine->callbacks.note_on) {
    engine->callbacks.note_on(note, velocity, now);
  }
  _schedule_note_off(engine, note, off_time);
}

static void _emit_sequence(arp_engine_t *engine, const uint8_t *sequence, const uint8_t *velocities, uint8_t count, systime_t now) {
  if (count == 0u) {
    return;
  }
  if (engine->config.pattern == ARP_PATTERN_CHORD || engine->config.strum_mode != ARP_STRUM_OFF) {
    systime_t offset = 0u;
    for (uint8_t i = 0u; i < count; ++i) {
      uint8_t idx = i;
      switch (engine->config.strum_mode) {
        case ARP_STRUM_UP: idx = i; break;
        case ARP_STRUM_DOWN: idx = (uint8_t)(count - 1u - i); break;
        case ARP_STRUM_ALT: idx = (i & 1u) ? (uint8_t)(count - 1u - i/2u) : (uint8_t)(i/2u); break;
        case ARP_STRUM_RANDOM: idx = (uint8_t)(_lcg_next(engine) % count); break;
        default: idx = i; break;
      }
      uint8_t vel = _accent_velocity(engine, velocities[idx], engine->step_index + i);
      uint8_t note = sequence[idx];
      _apply_lfo(engine, &note, &vel, now + offset);
      _queue_note_on(engine, note, vel, now + offset);
      if (engine->config.strum_mode != ARP_STRUM_OFF) {
        offset += engine->strum_offset;
      }
    }
  } else {
    uint8_t index = _resolve_direction_index(engine, count);
    if (index >= count) {
      index = (uint8_t)(count - 1u);
    }
    uint8_t vel = _accent_velocity(engine, velocities[index], engine->step_index);
    _emit_single_note(engine, sequence[index], vel, now);
  }
}

void arp_init(arp_engine_t *engine, const arp_config_t *cfg) {
  if (!engine) return;
  memset(engine, 0, sizeof(*engine));
  engine->random_seed = 0x12345u ^ (uint32_t)chVTGetSystemTimeX();
  if (cfg) {
    engine->config = *cfg;
  } else {
    arp_config_t def = {
      .enabled = false,
      .hold_enabled = false, // --- ARP FIX: Hold par défaut ---
      .rate = ARP_RATE_SIXTEENTH,
      .octave_range = 1u,
      .pattern = ARP_PATTERN_UP,
      .gate_percent = 60u,
      .swing_percent = 0u,
      .accent = ARP_ACCENT_OFF,
      .velocity_random = 0u,
      .strum_mode = ARP_STRUM_OFF,
      .strum_offset_ms = 0u,
      .repeat_count = 1u,
      .trigger_mode = ARP_TRIGGER_HOLD,
      .transpose = 0,
      .spread_percent = 0u,
      .octave_shift = 0,
      .direction_behavior = 0u,
      .pattern_select = 1u,
      .pattern_morph = 0u,
      .lfo_target = ARP_LFO_TARGET_GATE,
      .lfo_depth = 0u,
      .lfo_rate = 0u,
      .sync_mode = ARP_SYNC_INTERNAL
    };
    engine->config = def;
  }
  _sanitise_config(&engine->config);
  _recompute_periods(engine);
  engine->next_event = chVTGetSystemTimeX();
}

void arp_set_callbacks(arp_engine_t *engine, const arp_callbacks_t *cb) {
  if (!engine) return;
  if (cb) {
    engine->callbacks = *cb;
  } else {
    memset(&engine->callbacks, 0, sizeof(engine->callbacks));
  }
}

void arp_set_config(arp_engine_t *engine, const arp_config_t *cfg) {
  if (!engine || !cfg) return;
  arp_config_t tmp = *cfg;
  _sanitise_config(&tmp);
  engine->config = tmp;
  _recompute_periods(engine);
}

void arp_note_input(arp_engine_t *engine, uint8_t note, uint8_t velocity, bool pressed) {
  if (!engine) return;
  systime_t now = chVTGetSystemTimeX();
  if (pressed) {
    bool exists = false;
    uint8_t insert_at = engine->held_count;
    for (uint8_t i = 0u; i < engine->held_count; ++i) {
      if (engine->held_notes[i] == note) {
        engine->held_velocities[i] = velocity;
        exists = true;
        break;
      }
      if (engine->held_notes[i] > note) {
        insert_at = i;
        break;
      }
    }
    if (!exists && engine->held_count < ARP_ARRAY_SIZE(engine->held_notes)) {
      for (uint8_t j = engine->held_count; j > insert_at; --j) {
        engine->held_notes[j] = engine->held_notes[j-1u];
        engine->held_velocities[j] = engine->held_velocities[j-1u];
      }
      engine->held_notes[insert_at] = note;
      engine->held_velocities[insert_at] = velocity;
      engine->held_count++;
    }
    if (engine->config.trigger_mode == ARP_TRIGGER_RETRIG) {
      engine->running = false;
    }
    _try_start(engine, now);
  } else {
    for (uint8_t i = 0u; i < engine->held_count; ++i) {
      if (engine->held_notes[i] == note) {
        for (uint8_t j = i; j + 1u < engine->held_count; ++j) {
          engine->held_notes[j] = engine->held_notes[j+1u];
          engine->held_velocities[j] = engine->held_velocities[j+1u];
        }
        engine->held_count--;
        break;
      }
    }
    if (engine->config.trigger_mode != ARP_TRIGGER_FREERUN) {
      if (engine->held_count > 0u || !engine->config.hold_enabled) {
        engine->pattern_count = engine->held_count;
        for (uint8_t i = 0u; i < engine->pattern_count; ++i) {
          engine->pattern_notes[i] = engine->held_notes[i];
          engine->pattern_velocities[i] = engine->held_velocities[i];
        }
        engine->arp_notes_latched = (engine->pattern_count > 0u);
      }
    }
    _update_running_from_release(engine, now);
  }
}

void arp_tick(arp_engine_t *engine, systime_t now) {
  if (!engine) return;
  _recompute_periods(engine);
  _dispatch_pending_note_ons(engine, now);
  _dispatch_note_offs(engine, now);

  if (!engine->config.enabled) {
    return;
  }

  if (!engine->running) {
    _try_start(engine, now);
  }

  if (!engine->running) {
    return;
  }

  if (engine->next_event > now) {
    return;
  }

  uint8_t sequence[64];
  uint8_t velocities[64];
  const uint8_t seq_count = _build_sequence(engine, sequence, velocities);
  if (seq_count == 0u) {
    engine->next_event = now + engine->base_period;
    return;
  }

  if (engine->config.pattern != ARP_PATTERN_CHORD && engine->config.strum_mode == ARP_STRUM_OFF) {
    uint8_t index = _resolve_direction_index(engine, seq_count);
    if (index >= seq_count) index = (uint8_t)(seq_count - 1u);
    uint8_t vel = _accent_velocity(engine, velocities[index], engine->step_index);
    _emit_single_note(engine, sequence[index], vel, now);
  } else {
    _emit_sequence(engine, sequence, velocities, seq_count, now);
  }

  systime_t period = engine->base_period;
  if ((engine->step_index & 1u) && engine->config.swing_percent > 0u) {
    period += engine->swing_period;
  }
  engine->next_event = now + period;
  _advance_step(engine, seq_count);
}

void arp_stop_all(arp_engine_t *engine) {
  if (!engine) return;
  for (uint8_t i = 0u; i < engine->pending_on_count; ++i) {
    if (engine->callbacks.note_off) {
      engine->callbacks.note_off(engine->pending_on_notes[i]);
    }
  }
  engine->pending_on_count = 0u;
  _clear_active_notes(engine);
  engine->running = false;
  engine->next_event = chVTGetSystemTimeX();
}

void arp_set_hold(arp_engine_t *engine, bool enabled) {
  if (!engine) return;
  const bool previous = engine->config.hold_enabled;
  engine->config.hold_enabled = enabled ? true : false;
  if (!engine->config.hold_enabled) {
    if (engine->held_count == 0u) {
      arp_stop_all(engine); // --- ARP FIX: Hold Off → arrêt immédiat ---
      engine->pattern_count = 0u;
      engine->arp_notes_latched = false;
    }
  } else if (!previous && engine->pattern_count > 0u) {
    engine->arp_notes_latched = true;
  }
}
