/**
 * @file seq_engine_runner.c
 * @brief Bridge between the sequencer engine and runtime I/O backends.
 */

#include "seq_engine_runner.h"

#include <stdint.h>
#include <string.h>

#include "brick_config.h"
#include "cart_link.h"
#include "cart_registry.h"
#include "ch.h"
#include "midi.h"
#include "seq_engine.h"
#include "ui_mute_backend.h"

#ifdef BRICK_DEBUG_PLOCK
#include "chprintf.h"
#ifndef BRICK_DEBUG_PLOCK_STREAM
#define BRICK_DEBUG_PLOCK_STREAM ((BaseSequentialStream *)NULL)
#endif
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { \
        if (BRICK_DEBUG_PLOCK_STREAM != NULL) { \
            chprintf(BRICK_DEBUG_PLOCK_STREAM, "[PLOCK][%s] param=%u value=%u t=%lu\r\n", \
                     (tag), (unsigned)(param), (unsigned)(value), (unsigned long)(time)); \
        } \
    } while (0)
#else
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { (void)(tag); (void)(param); (void)(value); (void)(time); } while (0)
#endif

#ifndef SEQ_ENGINE_RUNNER_MIDI_CHANNEL
#define SEQ_ENGINE_RUNNER_MIDI_CHANNEL 0U
#endif

#ifndef SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS
#define SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS 24U
#endif

static CCM_DATA seq_engine_t s_engine;

typedef struct {
    bool active;
    cart_id_t cart;
    uint16_t param_id;
    uint8_t depth;
    uint8_t previous;
} seq_engine_runner_plock_state_t;

static CCM_DATA seq_engine_runner_plock_state_t s_plock_state[SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS];

static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time);
static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time);
static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time);
static bool _runner_track_muted(uint8_t track);
static uint8_t _runner_clamp_u8(int16_t value);
static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id);
static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id);
static void _runner_plock_release(seq_engine_runner_plock_state_t *slot);

void seq_engine_runner_init(seq_model_track_t *track) {
    seq_engine_callbacks_t callbacks = {
        .note_on = _runner_note_on_cb,
        .note_off = _runner_note_off_cb,
        .plock = _runner_plock_cb
    };

    seq_engine_config_t config = {
        .track = track,
        .callbacks = callbacks,
        .is_track_muted = _runner_track_muted
    };

    seq_engine_init(&s_engine, &config);
    memset(s_plock_state, 0, sizeof(s_plock_state));
}

void seq_engine_runner_attach_track(seq_model_track_t *track) {
    seq_engine_attach_track(&s_engine, track);
}

void seq_engine_runner_on_transport_play(void) {
    (void)seq_engine_start(&s_engine);
}

void seq_engine_runner_on_transport_stop(void) {
    seq_engine_stop(&s_engine);
    cart_id_t cart = cart_registry_get_active_id();
    if (cart < CART_COUNT) {
        for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
            seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
            if (!slot->active) {
                continue;
            }
            if (slot->cart == cart) {
                BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_RESTORE", slot->param_id, slot->previous, chVTGetSystemTimeX());
                cart_link_param_changed(slot->param_id, slot->previous, false, 0U);
            }
            _runner_plock_release(slot);
        }
    } else {
        for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
            _runner_plock_release(&s_plock_state[i]);
        }
    }

    // --- FIX: STOP doit forcer un All Notes Off immédiat sur tous les canaux actifs ---
    for (uint8_t ch = 0U; ch < 16U; ++ch) {
        midi_all_notes_off(MIDI_DEST_BOTH, ch);
    }
}

void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
    seq_engine_process_step(&s_engine, info);
}

static msg_t _runner_note_on_cb(const seq_engine_note_on_t *note_on, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_on == NULL) {
        return MSG_OK;
    }
    midi_note_on(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_on->note, note_on->velocity);
    return MSG_OK;
}

static msg_t _runner_note_off_cb(const seq_engine_note_off_t *note_off, systime_t scheduled_time) {
    (void)scheduled_time;
    if (note_off == NULL) {
        return MSG_OK;
    }
    midi_note_off(MIDI_DEST_BOTH, SEQ_ENGINE_RUNNER_MIDI_CHANNEL, note_off->note, 0U);
    return MSG_OK;
}

static msg_t _runner_plock_cb(const seq_engine_plock_t *plock, systime_t scheduled_time) {
    if ((plock == NULL) || (plock->plock.domain != SEQ_MODEL_PLOCK_CART)) {
        return MSG_OK;
    }

    const seq_model_plock_t *payload = &plock->plock;
    cart_id_t cart = cart_registry_get_active_id();
    if (cart >= CART_COUNT) {
        return MSG_OK;
    }

    const uint8_t value = _runner_clamp_u8(payload->value);

    switch (plock->action) {
    case SEQ_ENGINE_PLOCK_APPLY: {
        seq_engine_runner_plock_state_t *slot = _runner_plock_acquire(cart, payload->parameter_id);
        if (slot != NULL) {
            if (slot->depth == 0U) {
                slot->previous = cart_link_shadow_get(cart, payload->parameter_id);
            }
            if (slot->depth < UINT8_MAX) {
                slot->depth++;
            }
        }
        BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_APPLY", payload->parameter_id, value, scheduled_time);
        cart_link_param_changed(payload->parameter_id, value, false, 0U);
        break;
    }
    case SEQ_ENGINE_PLOCK_RESTORE: {
        seq_engine_runner_plock_state_t *slot = _runner_plock_find(cart, payload->parameter_id);
        if ((slot != NULL) && (slot->depth > 0U)) {
            slot->depth--;
            if (slot->depth == 0U) {
                BRICK_DEBUG_PLOCK_LOG("RUNNER_PLOCK_RESTORE", payload->parameter_id, slot->previous, scheduled_time);
                cart_link_param_changed(payload->parameter_id, slot->previous, false, 0U);
                _runner_plock_release(slot);
            }
        }
        break;
    }
    default:
        break;
    }

    return MSG_OK;
}

static bool _runner_track_muted(uint8_t track) {
    return ui_mute_backend_is_muted(track);
}

static uint8_t _runner_clamp_u8(int16_t value) {
    if (value < 0) {
        return 0U;
    }
    if (value > 127) {
        return 127U;
    }
    return (uint8_t)value;
}

static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id) {
    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
        seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
        if (slot->active && (slot->cart == cart) && (slot->param_id == param_id)) {
            return slot;
        }
    }
    return NULL;
}

static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id) {
    seq_engine_runner_plock_state_t *slot = _runner_plock_find(cart, param_id);
    if (slot != NULL) {
        return slot;
    }

    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
        slot = &s_plock_state[i];
        if (!slot->active) {
            slot->active = true;
            slot->cart = cart;
            slot->param_id = param_id;
            slot->depth = 0U;
            slot->previous = cart_link_shadow_get(cart, param_id);
            return slot;
        }
    }

    return NULL;
}

static void _runner_plock_release(seq_engine_runner_plock_state_t *slot) {
    if (slot == NULL) {
        return;
    }
    slot->active = false;
    slot->cart = (cart_id_t)0U;
    slot->param_id = 0U;
    slot->depth = 0U;
    slot->previous = 0U;
}
