/**
 * @file seq_engine_runner.c
 * @brief Reader-driven bridge between the sequencer runtime and MIDI/cart I/O.
 */

#include "seq_engine_runner.h"

#ifndef SEQ_USE_HANDLES
#error "SEQ_USE_HANDLES must be defined (firmware & host) for Reader-only runner"
#endif

#include <stdint.h>
#include <string.h>

#include "apps/midi_helpers.h"
#include "apps/midi_probe.h"
#include "apps/rtos_shim.h"
#include "apps/seq_led_bridge.h"
#include "brick_config.h"
#include "cart_link.h"
#include "cart_registry.h"
#include "core/seq/seq_access.h"
#include "core/seq/seq_midi_routing.h"
#include "core/seq/seq_plock_ids.h"
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
static CCM_DATA seq_engine_runner_note_state_t s_note_state[SEQ_ENGINE_RUNNER_TRACK_COUNT][SEQ_MODEL_VOICES_PER_STEP];

typedef struct {
    uint8_t ch;
    uint8_t note;
    uint8_t vel;
} midi_event_t;

static midi_event_t s_off_events[SEQ_ENGINE_RUNNER_TRACK_COUNT * SEQ_MODEL_VOICES_PER_STEP];
static midi_event_t s_on_events[SEQ_ENGINE_RUNNER_TRACK_COUNT * SEQ_MODEL_VOICES_PER_STEP];
static uint8_t s_off_count = 0U;
static uint8_t s_on_count = 0U;

typedef struct {
    bool active;
    uint8_t note;
    uint8_t velocity;
    uint8_t length;
    int8_t micro;
} seq_engine_runner_voice_ctx_t;

typedef struct {
    uint8_t track_index;
    uint32_t step_abs;
    uint8_t step_idx;
    seq_track_handle_t handle;
    const seq_model_step_t *model_step;
    seq_step_view_t step_view;
    seq_engine_runner_voice_ctx_t voices[SEQ_MODEL_VOICES_PER_STEP];
    int8_t all_transpose;
    int16_t all_velocity;
    int8_t all_length;
    int8_t all_micro;
    cart_id_t cart;
} seq_engine_runner_step_ctx_t;

static void _runner_reset_notes(void);
static void _runner_flush_active_notes(void);
static void _runner_advance_plock_state(void);
static void _runner_handle_step(uint8_t track,
                               uint32_t step_abs,
                               uint8_t step_idx,
                               seq_track_handle_t handle,
                               cart_id_t cart);
static void _runner_queue_event(uint8_t ch, uint8_t note, uint8_t velocity, bool is_on);
static void _runner_flush_queued_events(void);
static uint8_t _runner_clamp_u8(int32_t value);
static void _runner_send_note_on(uint8_t track, uint8_t note, uint8_t velocity) __attribute__((unused));
static void _runner_send_note_off(uint8_t track, uint8_t note);
static seq_engine_runner_plock_state_t *_runner_plock_find(cart_id_t cart, uint16_t param_id);
static seq_engine_runner_plock_state_t *_runner_plock_acquire(cart_id_t cart, uint16_t param_id);
static void _runner_plock_release(seq_engine_runner_plock_state_t *slot);
static void _runner_step_ctx_prepare(seq_engine_runner_step_ctx_t *ctx,
                                    uint8_t track,
                                    uint32_t step_abs,
                                    uint8_t step_idx,
                                    seq_track_handle_t handle,
                                    cart_id_t cart,
                                    const seq_step_view_t *view);
static void _runner_step_ctx_process_plocks(seq_engine_runner_step_ctx_t *ctx);
static void _runner_apply_midi_plock(seq_engine_runner_step_ctx_t *ctx, uint8_t id, uint8_t value, uint8_t flags);
static void _runner_apply_cart_plock(seq_engine_runner_step_ctx_t *ctx, uint8_t id, uint8_t value, uint8_t flags);
static void _runner_step_ctx_schedule(seq_engine_runner_step_ctx_t *ctx);

void seq_engine_runner_init(void) {
    _runner_reset_notes();
    memset(s_plock_state, 0, sizeof(s_plock_state));
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

    midi_probe_tick_begin(info->step_idx_abs);

    _runner_advance_plock_state();

    const uint32_t step_abs = info->step_idx_abs;
    const uint8_t step_idx = (uint8_t)(step_abs % SEQ_MODEL_STEPS_PER_TRACK);
    const cart_id_t cart = cart_registry_get_active_id();
    uint8_t bank = 0U;
    uint8_t pattern = 0U;
    seq_led_bridge_get_active(&bank, &pattern);

    for (uint8_t track = 0U; track < SEQ_ENGINE_RUNNER_TRACK_COUNT; ++track) {
        seq_track_handle_t handle = seq_reader_make_handle(bank, pattern, track);
        _runner_handle_step(track, step_abs, step_idx, handle, cart);
    }

    _runner_flush_queued_events();

    midi_probe_tick_end();
}

static void _runner_reset_notes(void) {
    for (uint8_t track = 0U; track < SEQ_ENGINE_RUNNER_TRACK_COUNT; ++track) {
        for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
            seq_engine_runner_note_state_t *state = &s_note_state[track][slot];
            state->active = false;
            state->note = 0U;
            state->off_step = 0U;
        }
    }
}

static void _runner_flush_active_notes(void) {
    for (uint8_t track = 0U; track < SEQ_ENGINE_RUNNER_TRACK_COUNT; ++track) {
        for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
            seq_engine_runner_note_state_t *state = &s_note_state[track][slot];
            if (state->active) {
                _runner_send_note_off(track, state->note);
                state->active = false;
                state->note = 0U;
                state->off_step = 0U;
            }
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

static void _runner_handle_step(uint8_t track,
                                uint32_t step_abs,
                                uint8_t step_idx,
                                seq_track_handle_t handle,
                                cart_id_t cart) {
    const uint8_t midi_ch = seq_midi_channel_for_track(track);
    for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
        seq_engine_runner_note_state_t *state = &s_note_state[track][slot];
        if (state->active && (step_abs >= state->off_step)) {
            _runner_queue_event(midi_ch, state->note, 0U, false);
            state->active = false;
            state->note = 0U;
            state->off_step = 0U;
        }
    }

    if (ui_mute_backend_is_muted(track)) {
        for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
            seq_engine_runner_note_state_t *state = &s_note_state[track][slot];
            if (state->active) {
                _runner_queue_event(midi_ch, state->note, 0U, false);
                state->active = false;
                state->note = 0U;
                state->off_step = 0U;
            }
        }
        return;
    }

    seq_step_view_t view;
    if (!seq_reader_get_step(handle, step_idx, &view)) {
        return;
    }

    seq_engine_runner_step_ctx_t ctx;
    _runner_step_ctx_prepare(&ctx, track, step_abs, step_idx, handle, cart, &view);

    if (ctx.model_step != NULL) {
        _runner_step_ctx_process_plocks(&ctx);
    }

    _runner_step_ctx_schedule(&ctx);
}

static void _runner_step_ctx_prepare(seq_engine_runner_step_ctx_t *ctx,
                                    uint8_t track,
                                    uint32_t step_abs,
                                    uint8_t step_idx,
                                    seq_track_handle_t handle,
                                    cart_id_t cart,
                                    const seq_step_view_t *view) {
    if ((ctx == NULL) || (view == NULL)) {
        return;
    }

    ctx->track_index = track;
    ctx->step_abs = step_abs;
    ctx->step_idx = step_idx;
    ctx->handle = handle;
    ctx->cart = cart;
    ctx->all_transpose = 0;
    ctx->all_velocity = 0;
    ctx->all_length = 0;
    ctx->all_micro = 0;
    ctx->step_view = *view;
    ctx->model_step = seq_reader_peek_step(handle, step_idx);
    if (ctx->model_step == NULL) {
        BRICK_DEBUG_PLOCK_LOG("debug", 0U, (uint32_t)step_idx, ctx->step_abs);
    }

    for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
        seq_engine_runner_voice_ctx_t *voice = &ctx->voices[slot];
        voice->active = false;
        voice->note = 0U;
        voice->velocity = 0U;
        voice->length = 1U;
        voice->micro = 0;

        seq_step_voice_view_t voice_view;
        if (!seq_reader_get_step_voice(handle, step_idx, slot, &voice_view)) {
            continue;
        }
        if (!voice_view.enabled) {
            continue;
        }

        voice->active = true;
        voice->note = _runner_clamp_u8((int32_t)voice_view.note);
        voice->velocity = _runner_clamp_u8((int32_t)voice_view.vel);
        voice->length = (voice_view.length == 0U) ? 1U : voice_view.length;
        voice->micro = voice_view.micro;
    }
}

static void _runner_step_ctx_process_plocks(seq_engine_runner_step_ctx_t *ctx) {
    if ((ctx == NULL) || (ctx->model_step == NULL)) {
        return;
    }

    seq_reader_pl_it_t it;
    if (seq_reader_pl_open(&it, ctx->model_step) <= 0) {
        return;
    }

    uint8_t id = 0U;
    uint8_t value = 0U;
    uint8_t flags = 0U;
    while (seq_reader_pl_next(&it, &id, &value, &flags) != 0) {
        const bool is_cart_domain = ((flags & SEQ_READER_PL_FLAG_DOMAIN_CART) != 0U);
        if (is_cart_domain || pl_is_cart(id)) {
            _runner_apply_cart_plock(ctx, id, value, flags);
        } else {
            _runner_apply_midi_plock(ctx, id, value, flags);
        }
    }
}

static void _runner_apply_midi_plock(seq_engine_runner_step_ctx_t *ctx, uint8_t id, uint8_t value, uint8_t flags) {
    (void)flags;
    if (ctx == NULL) {
        return;
    }

    BRICK_DEBUG_PLOCK_LOG("midi", id, value, ctx->step_abs);

    switch (id) {
        case PL_INT_ALL_TRANSP:
            ctx->all_transpose = pl_s8_from_u8(value);
            return;
        case PL_INT_ALL_VEL:
            ctx->all_velocity = (int16_t)pl_s8_from_u8(value);
            return;
        case PL_INT_ALL_LEN:
            ctx->all_length = pl_s8_from_u8(value);
            return;
        case PL_INT_ALL_MIC:
            ctx->all_micro = pl_s8_from_u8(value);
            return;
        case PL_INT_MIDI_CHANNEL:
            /* Routing is enforced by track index; ignore legacy channel override. */
            return;
        default:
            break;
    }

    uint8_t voice_index = 0U;
    seq_engine_runner_voice_ctx_t *voice = NULL;
    if ((id >= PL_INT_NOTE_V0) && (id <= PL_INT_NOTE_V3)) {
        voice_index = (uint8_t)(id - PL_INT_NOTE_V0);
        if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
            voice = &ctx->voices[voice_index];
            voice->note = _runner_clamp_u8((int32_t)value);
            voice->active = true;
        }
        return;
    }

    if ((id >= PL_INT_VEL_V0) && (id <= PL_INT_VEL_V3)) {
        voice_index = (uint8_t)(id - PL_INT_VEL_V0);
        if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
            voice = &ctx->voices[voice_index];
            voice->velocity = _runner_clamp_u8((int32_t)value);
            voice->active = (voice->velocity > 0U);
        }
        return;
    }

    if ((id >= PL_INT_LEN_V0) && (id <= PL_INT_LEN_V3)) {
        voice_index = (uint8_t)(id - PL_INT_LEN_V0);
        if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
            voice = &ctx->voices[voice_index];
            uint32_t len = (uint32_t)value;
            if (len == 0U) {
                len = 1U;
            }
            if (len > SEQ_MODEL_STEPS_PER_TRACK) {
                len = SEQ_MODEL_STEPS_PER_TRACK;
            }
            voice->length = (uint8_t)len;
        }
        return;
    }

    if ((id >= PL_INT_MIC_V0) && (id <= PL_INT_MIC_V3)) {
        voice_index = (uint8_t)(id - PL_INT_MIC_V0);
        if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
            voice = &ctx->voices[voice_index];
            voice->micro = pl_s8_from_u8(value);
        }
    }
}

static void _runner_apply_cart_plock(seq_engine_runner_step_ctx_t *ctx,
                                     uint8_t id,
                                     uint8_t value,
                                     uint8_t flags) {
    if ((ctx == NULL) || (ctx->cart >= CART_COUNT)) {
        return;
    }

    const bool legacy_cart_id = ((flags & SEQ_READER_PL_FLAG_DOMAIN_CART) != 0U);
    const uint16_t param_id = legacy_cart_id ? (uint16_t)id : (uint16_t)pl_cart_id(id);
    seq_engine_runner_plock_state_t *slot = _runner_plock_acquire(ctx->cart, param_id);
    if (slot == NULL) {
        return;
    }

    slot->depth = 1U;
    const uint8_t clamped = _runner_clamp_u8((int32_t)value);
    BRICK_DEBUG_PLOCK_LOG("cart", param_id, clamped, ctx->step_abs);
    cart_link_param_changed(param_id, clamped, false, 0U);
}

static void _runner_step_ctx_schedule(seq_engine_runner_step_ctx_t *ctx) {
    if (ctx == NULL) {
        return;
    }

    const uint8_t flags = ctx->step_view.flags;
    const bool has_voice = (flags & SEQ_STEPF_HAS_VOICE) != 0U;
    const bool automation_only = (flags & SEQ_STEPF_AUTOMATION_ONLY) != 0U;
    if (!has_voice || automation_only) {
        return;
    }

    const uint8_t midi_ch = seq_midi_channel_for_track(ctx->track_index);
    for (uint8_t slot = 0U; slot < SEQ_MODEL_VOICES_PER_STEP; ++slot) {
        seq_engine_runner_voice_ctx_t *voice = &ctx->voices[slot];
        if (!voice->active) {
            continue;
        }

        int32_t velocity = (int32_t)voice->velocity + ctx->all_velocity;
        if (velocity <= 0) {
            voice->active = false;
            continue;
        }
        if (velocity > 127) {
            velocity = 127;
        }
        uint8_t final_velocity = (uint8_t)velocity;

        int32_t note = (int32_t)voice->note + ctx->all_transpose;
        if (note < 0) {
            note = 0;
        } else if (note > 127) {
            note = 127;
        }
        uint8_t final_note = (uint8_t)note;

        int32_t length = (int32_t)voice->length + ctx->all_length;
        if (length < 1) {
            length = 1;
        } else if (length > (int32_t)SEQ_MODEL_STEPS_PER_TRACK) {
            length = (int32_t)SEQ_MODEL_STEPS_PER_TRACK;
        }
        uint32_t off_step = ctx->step_abs + (uint32_t)length;

        seq_engine_runner_note_state_t *state = &s_note_state[ctx->track_index][slot];
        if (state->active) {
            _runner_queue_event(midi_ch, state->note, 0U, false);
            state->active = false;
            state->note = 0U;
            state->off_step = 0U;
        }

        _runner_queue_event(midi_ch, final_note, final_velocity, true);

        state->active = true;
        state->note = final_note;
        state->off_step = off_step;
    }
}

static void _runner_queue_event(uint8_t ch, uint8_t note, uint8_t velocity, bool is_on) {
    midi_event_t *events = is_on ? s_on_events : s_off_events;
    uint8_t *count = is_on ? &s_on_count : &s_off_count;
    const uint8_t capacity = (uint8_t)(SEQ_ENGINE_RUNNER_TRACK_COUNT * SEQ_MODEL_VOICES_PER_STEP);

    if (*count >= capacity) {
        return;
    }

    midi_event_t *slot = &events[*count];
    slot->ch = ch;
    slot->note = note;
    slot->vel = velocity;
    (*count)++;
}

static void _runner_flush_queued_events(void) {
    for (uint8_t i = 0U; i < s_off_count; ++i) {
        const midi_event_t *event = &s_off_events[i];
        midi_note_off(event->ch, event->note, event->vel);
    }

    for (uint8_t i = 0U; i < s_on_count; ++i) {
        const midi_event_t *event = &s_on_events[i];
        midi_note_on(event->ch, event->note, event->vel);
    }

    s_off_count = 0U;
    s_on_count = 0U;
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

/* UNUSED: retained for potential diagnostic hooks. */
static void _runner_send_note_on(uint8_t track, uint8_t note, uint8_t velocity) {
    midi_note_on(seq_midi_channel_for_track(track), note, velocity);
}

static void _runner_send_note_off(uint8_t track, uint8_t note) {
    midi_note_off(seq_midi_channel_for_track(track), note, 0U);
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
