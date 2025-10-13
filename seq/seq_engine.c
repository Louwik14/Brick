/**
 * @file seq_engine.c
 * @brief Clock driven sequencer engine: playback + MIDI emission.
 * @ingroup seq_engine
 */

#include "seq_engine.h"

#include <string.h>

#include "ch.h"

#define DEFAULT_DESTINATION MIDI_DEST_BOTH

typedef struct {
    bool     gate_on;
    uint8_t  note;
    uint32_t note_off_step;
} seq_engine_voice_state_t;

static struct {
    seq_engine_config_t      cfg;
    seq_pattern_t            pattern;
    seq_runtime_t            runtime_cache;
    clock_step_handle_t      clock_handle;
    uint32_t                 playhead;
    bool                     running;
    seq_engine_voice_state_t voice_state[SEQ_MODEL_VOICE_COUNT];
    mutex_t                  pattern_mutex;
    uint8_t                  active_voice;
} g_engine;

static void seq_engine_publish_locked(void) {
    seq_runtime_snapshot_from_pattern(&g_engine.runtime_cache, &g_engine.pattern, g_engine.playhead);
    seq_runtime_publish(&g_engine.runtime_cache);
}

static uint16_t normalise_step_locked(uint8_t voice, uint32_t step_idx_abs) {
    uint16_t len = seq_model_voice_length(&g_engine.pattern, voice);
    if (len == 0) {
        len = SEQ_MODEL_STEP_COUNT;
    }
    return (uint16_t)(step_idx_abs % len);
}

static void send_all_notes_off_locked(void) {
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        seq_engine_voice_state_t *vs = &g_engine.voice_state[v];
        if (vs->gate_on) {
            midi_note_off(g_engine.cfg.dest, g_engine.cfg.midi_channel[v], vs->note, 0);
            vs->gate_on = false;
        }
    }
}

static void seq_engine_on_clock_step(const clock_step_info_t *info) {
    if (!info) {
        return;
    }
    if (!g_engine.running) {
        return;
    }

    if (chMtxTryLock(&g_engine.pattern_mutex) != MSG_OK) {
        return;
    }

    g_engine.playhead = info->step_idx_abs;
    seq_engine_publish_locked();

    const seq_runtime_t *snapshot = &g_engine.runtime_cache;

    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        seq_engine_voice_state_t *vs = &g_engine.voice_state[v];
        const seq_runtime_voice_t *rv = &snapshot->voices[v];
        uint16_t len = rv->length ? rv->length : SEQ_MODEL_STEP_COUNT;
        uint32_t local_step = len ? (info->step_idx_abs % len) : 0;
        const seq_runtime_step_t *rst = &rv->steps[local_step];

        if (vs->gate_on && info->step_idx_abs >= vs->note_off_step) {
            midi_note_off(g_engine.cfg.dest, g_engine.cfg.midi_channel[v], vs->note, 0);
            vs->gate_on = false;
        }

        if (rst->active) {
            if (vs->gate_on) {
                midi_note_off(g_engine.cfg.dest, g_engine.cfg.midi_channel[v], vs->note, 0);
            }
            midi_note_on(g_engine.cfg.dest, g_engine.cfg.midi_channel[v], rst->note, rst->velocity);
            vs->gate_on = true;
            vs->note    = rst->note;
            uint32_t off = info->step_idx_abs + (rst->length ? rst->length : 1u);
            vs->note_off_step = off;
        }
    }

    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_init(const seq_engine_config_t *cfg) {
    memset(&g_engine, 0, sizeof(g_engine));
    seq_model_init(&g_engine.pattern);
    chMtxObjectInit(&g_engine.pattern_mutex);
    g_engine.cfg.dest = DEFAULT_DESTINATION;
    for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
        g_engine.cfg.midi_channel[v] = v; /* default: channels 1..4 → 0-based */
    }
    if (cfg) {
        g_engine.cfg.dest = (cfg->dest == MIDI_DEST_NONE) ? DEFAULT_DESTINATION : cfg->dest;
        for (uint8_t v = 0; v < SEQ_MODEL_VOICE_COUNT; ++v) {
            uint8_t ch = cfg->midi_channel[v];
            if (ch == 0 || ch > 16) {
                ch = (uint8_t)(v + 1);
            }
            g_engine.cfg.midi_channel[v] = (uint8_t)(ch - 1u);
        }
    }
    g_engine.active_voice = 0;
    seq_runtime_snapshot_from_pattern(&g_engine.runtime_cache, &g_engine.pattern, 0);
    seq_runtime_publish(&g_engine.runtime_cache);
    g_engine.clock_handle = clock_manager_step_subscribe(seq_engine_on_clock_step);
}

void seq_engine_transport_start(void) {
    chMtxLock(&g_engine.pattern_mutex);
    g_engine.running = true;
    g_engine.playhead = 0;
    memset(g_engine.voice_state, 0, sizeof(g_engine.voice_state));
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_transport_stop(void) {
    chMtxLock(&g_engine.pattern_mutex);
    g_engine.running = false;
    send_all_notes_off_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_toggle_step(uint8_t voice, uint32_t step_idx_abs) {
    if (voice >= SEQ_MODEL_VOICE_COUNT) {
        voice = g_engine.active_voice;
    }
    chMtxLock(&g_engine.pattern_mutex);
    uint16_t local = normalise_step_locked(voice, step_idx_abs);
    seq_model_toggle_step(&g_engine.pattern, voice, local);
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_set_step_param_only(uint8_t voice, uint32_t step_idx_abs, bool on) {
    if (voice >= SEQ_MODEL_VOICE_COUNT) {
        voice = g_engine.active_voice;
    }
    chMtxLock(&g_engine.pattern_mutex);
    uint16_t local = normalise_step_locked(voice, step_idx_abs);
    seq_model_set_step_active(&g_engine.pattern, voice, local, !on);
    if (on) {
        /* ensure velocity zero to avoid stray gates */
        seq_model_set_step_param(&g_engine.pattern, voice, local, SEQ_PARAM_VELOCITY, 0, true);
    }
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_apply_plock_delta(seq_param_id_t param, int16_t delta, uint64_t step_mask) {
    if (param >= SEQ_PARAM_COUNT || delta == 0 || step_mask == 0) {
        return;
    }
    chMtxLock(&g_engine.pattern_mutex);
    for (uint16_t s = 0; s < 64; ++s) {
        if (step_mask & (1ull << s)) {
            uint16_t local = normalise_step_locked(g_engine.active_voice, s);
            bool     is_plocked = false;
            int16_t  current = seq_model_step_param(&g_engine.pattern,
                                                    g_engine.active_voice,
                                                    local,
                                                    param,
                                                    &is_plocked);
            int16_t  updated = current + delta;
            seq_model_set_step_param(&g_engine.pattern,
                                     g_engine.active_voice,
                                     local,
                                     param,
                                     updated,
                                     true);
        }
    }
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_set_active_voice(uint8_t voice) {
    if (voice >= SEQ_MODEL_VOICE_COUNT) {
        voice = 0;
    }
    g_engine.active_voice = voice;
}

void seq_engine_set_global_offset(seq_param_id_t param, int16_t value) {
    if (param >= SEQ_PARAM_COUNT) {
        return;
    }
    chMtxLock(&g_engine.pattern_mutex);
    seq_offsets_t offsets = g_engine.pattern.offsets;
    switch (param) {
        case SEQ_PARAM_NOTE: offsets.transpose = value; break;
        case SEQ_PARAM_VELOCITY: offsets.velocity = value; break;
        case SEQ_PARAM_LENGTH: offsets.length = value; break;
        case SEQ_PARAM_MICRO_TIMING: offsets.micro_timing = value; break;
        default: break;
    }
    seq_model_set_offsets(&g_engine.pattern, &offsets);
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_set_voice_length(uint8_t voice, uint16_t length) {
    if (voice >= SEQ_MODEL_VOICE_COUNT) {
        return;
    }
    chMtxLock(&g_engine.pattern_mutex);
    seq_model_voice_set_length(&g_engine.pattern, voice, length);
    seq_engine_publish_locked();
    chMtxUnlock(&g_engine.pattern_mutex);
}

void seq_engine_set_voice_channel(uint8_t voice, uint8_t channel) {
    if (voice >= SEQ_MODEL_VOICE_COUNT) {
        return;
    }
    if (channel == 0) {
        channel = 1;
    }
    if (channel > 16) {
        channel = 16;
    }
    g_engine.cfg.midi_channel[voice] = (uint8_t)(channel - 1u);
}
