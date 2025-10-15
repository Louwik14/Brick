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
  if (cfg->repeat_count == 0u) cfg->repeat_count = 1u;
  if (cfg->repeat_count > 4u) cfg->repeat_count = 4u;
  if (cfg->strum_offset_ms > 60u) cfg->strum_offset_ms = 60u;
  if (cfg->vel_accent > 127u) cfg->vel_accent = 127u;
  if (cfg->transpose < -12) cfg->transpose = -12;
  if (cfg->transpose > 12) cfg->transpose = 12;
  if (cfg->spread_percent > 100u) cfg->spread_percent = 100u;
  if (cfg->direction_behavior > 2u) cfg->direction_behavior = (cfg->direction_behavior % 3u);
  if (cfg->rate >= ARP_RATE_COUNT) cfg->rate = ARP_RATE_SIXTEENTH;
  if (cfg->pattern >= ARP_PATTERN_COUNT) cfg->pattern = ARP_PATTERN_UP;
  if (cfg->accent >= ARP_ACCENT_COUNT) cfg->accent = ARP_ACCENT_OFF;
  if (cfg->strum_mode >= ARP_STRUM_COUNT) cfg->strum_mode = ARP_STRUM_OFF;
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

// --- ARP FIX: helpers pour les groupes Hold / Strum ---
static void _copy_notes(uint8_t *dst_notes, uint8_t *dst_vel, uint8_t *dst_count,
                        const uint8_t *src_notes, const uint8_t *src_vel, uint8_t src_count) {
  if (!dst_count) {
    return;
  }
  if (dst_notes && src_notes) {
    for (uint8_t i = 0u; i < src_count; ++i) {
      dst_notes[i] = src_notes[i];
      if (dst_vel && src_vel) {
        dst_vel[i] = src_vel[i];
      } else if (dst_vel) {
        dst_vel[i] = 0u;
      }
    }
  }
  *dst_count = src_count;
}

static void _insert_sorted_unique(uint8_t *notes, uint8_t *vel, uint8_t *count,
                                  uint8_t capacity, uint8_t note, uint8_t velocity) {
  if (!notes || !count || *count >= capacity) {
    return;
  }
  uint8_t idx = 0u;
  while (idx < *count && notes[idx] < note) {
    ++idx;
  }
  if (idx < *count && notes[idx] == note) {
    if (vel) {
      vel[idx] = velocity;
    }
    return;
  }
  for (uint8_t j = *count; j > idx; --j) {
    notes[j] = notes[j - 1u];
    if (vel) {
      vel[j] = vel[j - 1u];
    }
  }
  notes[idx] = note;
  if (vel) {
    vel[idx] = velocity;
  }
  (*count)++;
}

static void _remove_note(uint8_t *notes, uint8_t *vel, uint8_t *count, uint8_t note) {
  if (!notes || !count) {
    return;
  }
  for (uint8_t i = 0u; i < *count; ++i) {
    if (notes[i] == note) {
      for (uint8_t j = i; j + 1u < *count; ++j) {
        notes[j] = notes[j + 1u];
        if (vel) {
          vel[j] = vel[j + 1u];
        }
      }
      if (*count > 0u) {
        (*count)--;
      }
      break;
    }
  }
}

static void _activate_from_phys(arp_engine_t *engine) {
  _copy_notes(engine->pattern_notes, engine->pattern_velocities, &engine->pattern_count,
              engine->phys_notes, engine->phys_velocities, engine->phys_count);
  if (engine->config.hold_enabled) {
    engine->latched_active = (engine->latched_count > 0u);
  } else {
    engine->latched_active = false;
  }
}

static void _activate_from_latched(arp_engine_t *engine) {
  _copy_notes(engine->pattern_notes, engine->pattern_velocities, &engine->pattern_count,
              engine->latched_notes, engine->latched_velocities, engine->latched_count);
  engine->latched_active = (engine->latched_count > 0u);
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
  engine->strum_phase = 0u;
  engine->latched_active = (engine->latched_count > 0u);
}

static void _try_start(arp_engine_t *engine, systime_t now) {
  if (!engine->config.enabled) {
    engine->running = false;
    return;
  }

  if (engine->pattern_count == 0u) {
    if (engine->config.hold_enabled && engine->latched_count > 0u) {
      _activate_from_latched(engine);
    } else if (engine->phys_count > 0u) {
      if (engine->config.hold_enabled) {
        _copy_notes(engine->latched_notes, engine->latched_velocities, &engine->latched_count,
                    engine->phys_notes, engine->phys_velocities, engine->phys_count);
        _activate_from_latched(engine);
      } else {
        _activate_from_phys(engine);
      }
    }
  }

  if (engine->pattern_count == 0u) {
    engine->running = false;
    return;
  }

  if (!engine->running) {
    _reset_runtime(engine, now);
    engine->running = true;
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

static uint8_t _accent_velocity(arp_engine_t *engine, uint8_t base, uint32_t step) {
  if (!engine) {
    return base;
  }
  if (engine->config.accent == ARP_ACCENT_OFF || engine->config.vel_accent == 0u) {
    return base;
  }
  int32_t delta = 0;
  switch (engine->config.accent) {
    case ARP_ACCENT_FIRST:
      if (step == 0u) {
        delta = engine->config.vel_accent;
      }
      break;
    case ARP_ACCENT_ALTERNATE:
      if ((step & 1u) == 0u) {
        delta = engine->config.vel_accent;
      }
      break;
    case ARP_ACCENT_RANDOM:
      delta = (int32_t)(_lcg_next(engine) % (uint32_t)(engine->config.vel_accent + 1u));
      break;
    default:
      break;
  }
  return _clamp_u7((int32_t)base + delta);
}

static uint8_t _apply_strum_variation(arp_engine_t *engine, uint8_t velocity) {
  if (!engine || engine->config.strum_mode == ARP_STRUM_OFF) {
    return velocity;
  }
  uint8_t percent = (uint8_t)(5u + (_lcg_next(engine) % 6u)); // 5..10%
  int32_t delta = ((int32_t)velocity * (int32_t)percent) / 100;
  if ((_lcg_next(engine) & 1u) != 0u) {
    return _clamp_u7((int32_t)velocity + delta);
  }
  int32_t lowered = (int32_t)velocity - delta;
  if (lowered < 1) {
    lowered = 1;
  }
  return (uint8_t)lowered;
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
  const int32_t transpose = (int32_t)engine->config.transpose; // --- ARP FIX: octave shift retiré, transpose en demi-tons ---
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
  return count;
}

static void _emit_single_note(arp_engine_t *engine, uint8_t note, uint8_t velocity, systime_t now) {
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
    uint8_t order[64];
    for (uint8_t i = 0u; i < count; ++i) {
      order[i] = i;
    }
    switch (engine->config.strum_mode) {
      case ARP_STRUM_DOWN:
        for (uint8_t i = 0u; i < count / 2u; ++i) {
          uint8_t j = (uint8_t)(count - 1u - i);
          uint8_t tmp = order[i];
          order[i] = order[j];
          order[j] = tmp;
        }
        break;
      case ARP_STRUM_ALT: {
        bool down = (engine->strum_phase & 0x1u) != 0u;
        engine->strum_phase ^= 0x1u;
        if (down) {
          for (uint8_t i = 0u; i < count / 2u; ++i) {
            uint8_t j = (uint8_t)(count - 1u - i);
            uint8_t tmp = order[i];
            order[i] = order[j];
            order[j] = tmp;
          }
        }
        break;
      }
      case ARP_STRUM_RANDOM:
        for (uint8_t i = count; i > 1u; --i) {
          uint8_t j = (uint8_t)(_lcg_next(engine) % i);
          uint8_t tmp = order[i - 1u];
          order[i - 1u] = order[j];
          order[j] = tmp;
        }
        break;
      case ARP_STRUM_UP:
      case ARP_STRUM_OFF:
      default:
        break;
    }

    systime_t offset = 0u;
    for (uint8_t i = 0u; i < count; ++i) {
      const uint8_t idx = order[i];
      uint8_t vel = _accent_velocity(engine, velocities[idx], engine->step_index + i);
      vel = _apply_strum_variation(engine, vel);
      systime_t target_time = now + offset;
      if (engine->config.strum_mode == ARP_STRUM_RANDOM && engine->strum_offset > 0u) {
        systime_t jitter_max = engine->strum_offset / 3u;
        if (jitter_max > 0u) {
          systime_t jitter = (systime_t)(_lcg_next(engine) % (jitter_max + 1u));
          if ((_lcg_next(engine) & 1u) != 0u) {
            if (jitter < target_time - now) {
              target_time -= jitter;
            }
          } else {
            target_time += jitter;
          }
        }
      }
      _queue_note_on(engine, sequence[idx], vel, target_time);
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
      .vel_accent = 64u,
      .strum_mode = ARP_STRUM_OFF,
      .strum_offset_ms = 0u,
      .repeat_count = 1u,
      .transpose = 0,
      .spread_percent = 0u,
      .direction_behavior = 0u,
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
    const bool had_phys = (engine->phys_count > 0u);
    _insert_sorted_unique(engine->phys_notes, engine->phys_velocities, &engine->phys_count,
                          ARP_ARRAY_SIZE(engine->phys_notes), note, velocity);
    if (engine->config.hold_enabled) {
      // --- ARP FIX: hold group latch (ajout progressif tant que touches physiques actives) ---
      if (!had_phys) {
        engine->latched_count = 0u;
      }
      if (engine->latched_count == 0u) {
        _copy_notes(engine->latched_notes, engine->latched_velocities, &engine->latched_count,
                    engine->phys_notes, engine->phys_velocities, engine->phys_count);
      } else if (had_phys) {
        _insert_sorted_unique(engine->latched_notes, engine->latched_velocities, &engine->latched_count,
                              ARP_ARRAY_SIZE(engine->latched_notes), note, velocity);
      }
      _activate_from_latched(engine);
    } else {
      // --- ARP FIX: mode direct (Hold off) → pattern = notes physiques ---
      _activate_from_phys(engine);
    }
    _try_start(engine, now);
  } else {
    _remove_note(engine->phys_notes, engine->phys_velocities, &engine->phys_count, note);
    if (engine->config.hold_enabled) {
      if (engine->phys_count == 0u) {
        engine->latched_active = (engine->latched_count > 0u);
      }
    } else {
      // --- ARP FIX: Hold off → on suit uniquement les notes physiques restantes ---
      _activate_from_phys(engine);
      if (engine->phys_count == 0u) {
        engine->pattern_count = 0u;
        engine->running = false;
        engine->next_event = now;
      }
    }
  }
  _try_start(engine, now);
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
    engine->latched_count = 0u;
    engine->latched_active = false;
    if (engine->phys_count == 0u) {
      arp_stop_all(engine); // --- ARP FIX: Hold Off → arrêt immédiat ---
      engine->pattern_count = 0u;
    } else {
      _activate_from_phys(engine);
    }
  } else if (!previous) {
    _copy_notes(engine->latched_notes, engine->latched_velocities, &engine->latched_count,
                engine->phys_notes, engine->phys_velocities, engine->phys_count);
    if (engine->latched_count == 0u && engine->pattern_count > 0u) {
      _copy_notes(engine->latched_notes, engine->latched_velocities, &engine->latched_count,
                  engine->pattern_notes, engine->pattern_velocities, engine->pattern_count);
    }
    engine->latched_active = (engine->latched_count > 0u);
  }
}
