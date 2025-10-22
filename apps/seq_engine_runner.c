/**
 * @file seq_engine_runner.c
 * @brief Reader-driven bridge between the sequencer runtime and MIDI/cart I/O.
 */

#include "seq_engine_runner.h"

#include <stdint.h>
#include <string.h>

#include "apps/midi_helpers.h"
#include "apps/rtos_shim.h"
#include "brick_config.h"
#include "cart_link.h"
#include "cart_registry.h"
#include "core/seq/seq_access.h"
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

#ifndef SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS
#define SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS 24U
#endif

typedef struct {
    bool active;
    cart_id_t cart;
    uint16_t param_id;
    uint8_t depth;
    uint8_t previous;
} seq_engine_runner_plock_state_t;

typedef struct {
    bool active;
    uint8_t note;
    uint32_t off_step;
} seq_engine_runner_note_state_t;

enum {
    SEQ_ENGINE_RUNNER_TRACK_COUNT = 16U
};

static CCM_DATA seq_engine_runner_plock_state_t s_plock_state[SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS];
static CCM_DATA seq_engine_runner_note_state_t s_note_state[SEQ_ENGINE_RUNNER_TRACK_COUNT];

static void _runner_reset_notes(void);
static void _runner_flush_active_notes(void);
static void _runner_advance_plock_state(void);
static void _runner_apply_plocks(seq_track_handle_t handle, uint8_t step_idx, cart_id_t cart);
static void _runner_handle_step(uint8_t track, uint32_t step_abs, uint8_t step_idx, seq_track_handle_t handle);
static uint8_t _runner_clamp_u8(int32_t value);
static void _runner_send_note_on(uint8_t track, uint8_t note, uint8_t velocity);
static void _runner_send_note_off(uint8_t track, uint8_t note);
static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id);
static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id);
static void _runner_plock_release(seq_engine_runner_plock_state_t *slot);

void seq_engine_runner_init(seq_model_track_t *track) {
    (void)track;
    _runner_reset_notes();
    memset(s_plock_state, 0, sizeof(s_plock_state));
}

void seq_engine_runner_attach_track(seq_model_track_t *track) {
    (void)track;
    _runner_flush_active_notes();
    _runner_reset_notes();
    _runner_advance_plock_state();
    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
        _runner_plock_release(&s_plock_state[i]);
    }
}

void seq_engine_runner_on_transport_play(void) {
    _runner_flush_active_notes();
    _runner_reset_notes();
    _runner_advance_plock_state();
}

void seq_engine_runner_on_transport_stop(void) {
    _runner_flush_active_notes();

    cart_id_t cart = cart_registry_get_active_id();
    if (cart < CART_COUNT) {
        for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
            seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
            if (!slot->active || (slot->cart != cart)) {
                continue;
            }
            cart_link_param_changed(slot->param_id, slot->previous, false, 0U);
            _runner_plock_release(slot);
        }
    } else {
        for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
            _runner_plock_release(&s_plock_state[i]);
        }
    }

    for (uint8_t ch = 1U; ch <= SEQ_ENGINE_RUNNER_TRACK_COUNT; ++ch) {
        midi_all_notes_off(ch);
    }
}

void seq_engine_runner_on_clock_step(const clock_step_info_t *info) {
    if (info == NULL) {
        return;
    }

    _runner_advance_plock_state();

    const uint32_t step_abs = info->step_idx_abs;
    const uint8_t step_idx = (uint8_t)(step_abs % SEQ_MODEL_STEPS_PER_TRACK);
    const seq_track_handle_t active = seq_reader_get_active_track_handle();
    const cart_id_t cart = cart_registry_get_active_id();

    for (uint8_t track = 0U; track < SEQ_ENGINE_RUNNER_TRACK_COUNT; ++track) {
        seq_track_handle_t handle = seq_reader_make_handle(active.bank, active.pattern, track);
        _runner_handle_step(track, step_abs, step_idx, handle);
        _runner_apply_plocks(handle, step_idx, cart);
    }
}

static void _runner_reset_notes(void) {
    memset(s_note_state, 0, sizeof(s_note_state));
}

static void _runner_flush_active_notes(void) {
    for (uint8_t track = 0U; track < SEQ_ENGINE_RUNNER_TRACK_COUNT; ++track) {
        seq_engine_runner_note_state_t *state = &s_note_state[track];
        if (state->active) {
            _runner_send_note_off(track, state->note);
            state->active = false;
        }
    }
}

static void _runner_advance_plock_state(void) {
    cart_id_t cart = cart_registry_get_active_id();
    for (size_t i = 0U; i < SEQ_ENGINE_RUNNER_MAX_ACTIVE_PLOCKS; ++i) {
        seq_engine_runner_plock_state_t *slot = &s_plock_state[i];
        if (!slot->active) {
            continue;
        }
        if (slot->depth > 0U) {
            slot->depth--;
        }
        if ((slot->depth == 0U) && (cart < CART_COUNT) && (slot->cart == cart)) {
            cart_link_param_changed(slot->param_id, slot->previous, false, 0U);
            _runner_plock_release(slot);
        } else if (cart >= CART_COUNT) {
            _runner_plock_release(slot);
        }
    }
}

static bool _runner_step_has_voice(const seq_step_view_t *view) {
    if (view == NULL) {
        return false;
    }
    const uint8_t flags = view->flags;
    const bool automation_only = (flags & SEQ_STEPF_AUTOMATION_ONLY) != 0U;
    const bool has_voice = (flags & SEQ_STEPF_HAS_VOICE) != 0U;
    return has_voice && !automation_only;
}

static void _runner_handle_step(uint8_t track,
                                uint32_t step_abs,
                                uint8_t step_idx,
                                seq_track_handle_t handle) {
    seq_engine_runner_note_state_t *state = &s_note_state[track];

    if (state->active && (step_abs >= state->off_step)) {
        _runner_send_note_off(track, state->note);
        state->active = false;
    }

    if (ui_mute_backend_is_muted(track)) {
        if (state->active) {
            _runner_send_note_off(track, state->note);
            state->active = false;
        }
        return;
    }

    seq_step_view_t view;
    if (!seq_reader_get_step(handle, step_idx, &view)) {
        return;
    }

    if (!_runner_step_has_voice(&view) || (view.vel == 0U)) {
        return;
    }

    uint8_t note = _runner_clamp_u8((int32_t)view.note);
    uint8_t velocity = _runner_clamp_u8((int32_t)view.vel);
    if (velocity == 0U) {
        return;
    }

    if (state->active) {
        _runner_send_note_off(track, state->note);
        state->active = false;
    }

    _runner_send_note_on(track, note, velocity);

    state->active = true;
    state->note = note;
    uint32_t length = view.length;
    if (length == 0U) {
        length = 1U;
    }
    state->off_step = step_abs + length;
}

static bool _runner_is_cart_param(uint16_t param_id) {
    return (param_id & 0x8000U) == 0U;
}

static void _runner_apply_plocks(seq_track_handle_t handle, uint8_t step_idx, cart_id_t cart) {
    if (cart >= CART_COUNT) {
        return;
    }

    seq_step_view_t view;
    if (!seq_reader_get_step(handle, step_idx, &view)) {
        return;
    }

    if ((view.flags & SEQ_STEPF_HAS_CART_PLOCK) == 0U) {
        return;
    }

    seq_plock_iter_t it;
    if (!seq_reader_plock_iter_open(handle, step_idx, &it)) {
        return;
    }

    uint16_t param_id;
    int32_t value;
    while (seq_reader_plock_iter_next(&it, &param_id, &value)) {
        if (!_runner_is_cart_param(param_id)) {
            continue;
        }
        seq_engine_runner_plock_state_t *slot = _runner_plock_acquire(cart, param_id);
        if (slot == NULL) {
            continue;
        }
        if (slot->depth == 0U) {
            slot->previous = cart_link_shadow_get(cart, param_id);
        }
        slot->depth = 1U;
        const uint8_t clamped = _runner_clamp_u8(value);
        cart_link_param_changed(param_id, clamped, false, 0U);
    }
}

static uint8_t _runner_clamp_u8(int32_t value) {
    if (value < 0) {
        return 0U;
    }
    if (value > 127) {
        return 127U;
    }
    return (uint8_t)value;
}

static void _runner_send_note_on(uint8_t track, uint8_t note, uint8_t velocity) {
    midi_note_on((uint8_t)(track + 1U), note, velocity);
}

static void _runner_send_note_off(uint8_t track, uint8_t note) {
    midi_note_off((uint8_t)(track + 1U), note, 0U);
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
