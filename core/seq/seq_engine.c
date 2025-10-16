/**
 * @file seq_engine.c
 * @brief Sequencer engine implementation (reader + scheduler + player).
 */

#include "seq_engine.h"
#include "brick_config.h"

#include <limits.h>
#include <string.h>

#ifdef BRICK_DEBUG_PLOCK
#include "chprintf.h"
#ifndef BRICK_DEBUG_PLOCK_STREAM
#define BRICK_DEBUG_PLOCK_STREAM ((BaseSequentialStream *)NULL)
#endif
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { \
        if (BRICK_DEBUG_PLOCK_STREAM != NULL) { \
            chprintf(BRICK_DEBUG_PLOCK_STREAM, "[PLOCK][%s] param=%u value=%ld t=%lu\r\n", \
                     (tag), (unsigned)(param), (long)(value), (unsigned long)(time)); \
        } \
    } while (0)
#else
#define BRICK_DEBUG_PLOCK_LOG(tag, param, value, time) \
    do { (void)(tag); (void)(param); (void)(value); (void)(time); } while (0)
#endif

#define SEQ_ENGINE_PLAYER_STACK_SIZE   768U
#define SEQ_ENGINE_MICRO_MAX           12
#define SEQ_ENGINE_MICRO_DIVISOR       24

static CCM_DATA THD_WORKING_AREA(s_seq_engine_player_wa, SEQ_ENGINE_PLAYER_STACK_SIZE);

static void _seq_engine_reader_init(seq_engine_reader_t *reader, const seq_model_pattern_t *pattern);
static void _seq_engine_reader_refresh_flags(seq_engine_reader_t *reader);
static void _seq_engine_player_init(seq_engine_player_t *player);
static THD_FUNCTION(_seq_engine_player_thread, arg);
static void _seq_engine_dispatch_event(seq_engine_t *engine, const seq_engine_event_t *event);
static void _seq_engine_signal_player(seq_engine_t *engine);
static bool _seq_engine_schedule_event(seq_engine_t *engine, const seq_engine_event_t *event);
static void _seq_engine_all_notes_off(seq_engine_t *engine);
static void _seq_engine_reset_voice_state(seq_engine_t *engine);
static bool _seq_engine_is_track_muted(const seq_engine_t *engine, uint8_t track);
static uint8_t _seq_engine_apply_scale(uint8_t note, const seq_model_scale_config_t *scale);
static int64_t _seq_engine_micro_to_delta(systime_t step_duration, int micro);
static systime_t _seq_engine_saturate_time(int64_t value);
static void _seq_engine_schedule_plocks(seq_engine_t *engine,
                                        const seq_model_step_t *step,
                                        systime_t apply_time,
                                        systime_t restore_time);
static void _seq_engine_handle_step(seq_engine_t *engine,
                                    const seq_model_step_t *step,
                                    const clock_step_info_t *info,
                                    size_t step_index);

void seq_engine_init(seq_engine_t *engine, const seq_engine_config_t *config) {
    chDbgCheck(engine != NULL);

    memset(engine, 0, sizeof(*engine));

    if (config != NULL) {
        engine->config = *config;
    }

    _seq_engine_reader_init(&engine->reader, engine->config.pattern);
    seq_engine_scheduler_clear(&engine->scheduler);
    _seq_engine_player_init(&engine->player);
    chMtxObjectInit(&engine->scheduler_lock);
    chBSemObjectInit(&engine->player_sem, true);
    engine->clock_attached = false;
    _seq_engine_reset_voice_state(engine);
}

void seq_engine_set_callbacks(seq_engine_t *engine, const seq_engine_callbacks_t *callbacks) {
    chDbgCheck(engine != NULL);

    if (callbacks != NULL) {
        engine->config.callbacks = *callbacks;
    } else {
        memset(&engine->config.callbacks, 0, sizeof(engine->config.callbacks));
    }
}

void seq_engine_attach_pattern(seq_engine_t *engine, seq_model_pattern_t *pattern) {
    chDbgCheck(engine != NULL);

    engine->config.pattern = pattern;
    _seq_engine_reader_init(&engine->reader, pattern);
}

msg_t seq_engine_start(seq_engine_t *engine) {
    chDbgCheck(engine != NULL);

    if (engine->clock_attached) {
        return MSG_OK;
    }

    engine->clock_attached = true;
    engine->player.running = true;
    _seq_engine_reset_voice_state(engine);

    if (engine->player.thread == NULL) {
        engine->player.thread = chThdCreateStatic(s_seq_engine_player_wa,
                                                  sizeof(s_seq_engine_player_wa),
                                                  NORMALPRIO + 1,
                                                  _seq_engine_player_thread,
                                                  engine);
        if (engine->player.thread == NULL) {
            engine->player.running = false;
            engine->clock_attached = false;
            return MSG_RESET;
        }
    } else {
        _seq_engine_signal_player(engine);
    }

    return MSG_OK;
}

void seq_engine_stop(seq_engine_t *engine) {
    chDbgCheck(engine != NULL);

    if (!engine->clock_attached) {
        return;
    }

    engine->clock_attached = false;
    engine->player.running = false;

    chMtxLock(&engine->scheduler_lock);
    seq_engine_scheduler_clear(&engine->scheduler);
    chMtxUnlock(&engine->scheduler_lock);
    _seq_engine_signal_player(engine);

    if (engine->player.thread != NULL) {
        chThdWait(engine->player.thread);
        engine->player.thread = NULL;
    }

    _seq_engine_all_notes_off(engine); // --- FIX: couper immédiatement les notes encore actives lors d'un STOP ---
    _seq_engine_reset_voice_state(engine);
}

void seq_engine_reset(seq_engine_t *engine) {
    chDbgCheck(engine != NULL);

    chMtxLock(&engine->scheduler_lock);
    seq_engine_scheduler_clear(&engine->scheduler);
    chMtxUnlock(&engine->scheduler_lock);

    _seq_engine_reader_init(&engine->reader, engine->config.pattern);
    _seq_engine_reset_voice_state(engine);
}

bool seq_engine_scheduler_push(seq_engine_scheduler_t *scheduler, const seq_engine_event_t *event) {
    chDbgCheck((scheduler != NULL) && (event != NULL));

    if (scheduler->count >= SEQ_ENGINE_SCHEDULER_CAPACITY) {
        return false;
    }

    size_t insert_offset = scheduler->count;
    for (size_t i = 0U; i < scheduler->count; ++i) {
        size_t idx = (scheduler->head + i) % SEQ_ENGINE_SCHEDULER_CAPACITY;
        if (scheduler->buffer[idx].scheduled_time > event->scheduled_time) {
            insert_offset = i;
            break;
        }
    }

    const size_t insert_index = (scheduler->head + insert_offset) % SEQ_ENGINE_SCHEDULER_CAPACITY;
    size_t tail_index = (scheduler->head + scheduler->count) % SEQ_ENGINE_SCHEDULER_CAPACITY;

    if (insert_offset == scheduler->count) {
        scheduler->buffer[tail_index] = *event;
    } else {
        size_t cur = tail_index;
        while (cur != insert_index) {
            size_t prev = (cur == 0U) ? (SEQ_ENGINE_SCHEDULER_CAPACITY - 1U) : (cur - 1U);
            scheduler->buffer[cur] = scheduler->buffer[prev];
            cur = prev;
        }
        scheduler->buffer[insert_index] = *event;
    }

    scheduler->count++;
    return true;
}

bool seq_engine_scheduler_pop(seq_engine_scheduler_t *scheduler, seq_engine_event_t *out_event) {
    chDbgCheck(scheduler != NULL);

    if (scheduler->count == 0U) {
        return false;
    }

    if (out_event != NULL) {
        *out_event = scheduler->buffer[scheduler->head];
    }

    scheduler->head = (scheduler->head + 1U) % SEQ_ENGINE_SCHEDULER_CAPACITY;
    scheduler->count--;
    return true;
}

bool seq_engine_scheduler_peek(const seq_engine_scheduler_t *scheduler, seq_engine_event_t *out_event) {
    chDbgCheck(scheduler != NULL);

    if (scheduler->count == 0U) {
        return false;
    }

    if (out_event != NULL) {
        *out_event = scheduler->buffer[scheduler->head];
    }

    return true;
}

void seq_engine_scheduler_clear(seq_engine_scheduler_t *scheduler) {
    chDbgCheck(scheduler != NULL);

    scheduler->head = 0U;
    scheduler->count = 0U;
    memset(scheduler->buffer, 0, sizeof(scheduler->buffer));
}

void seq_engine_process_step(seq_engine_t *engine, const clock_step_info_t *info) {
    if ((engine == NULL) || (info == NULL)) {
        return;
    }

    if (!engine->clock_attached || (engine->config.pattern == NULL)) {
        return;
    }

    seq_engine_reader_t *reader = &engine->reader;
    reader->pattern = engine->config.pattern;
    reader->step_index = (size_t)(info->step_idx_abs % SEQ_MODEL_STEPS_PER_PATTERN);
    _seq_engine_reader_refresh_flags(reader);

    const seq_model_step_t *step = &engine->config.pattern->steps[reader->step_index];
    _seq_engine_handle_step(engine, step, info, reader->step_index);
    reader->last_generation = engine->config.pattern->generation;
}

static void _seq_engine_reader_init(seq_engine_reader_t *reader, const seq_model_pattern_t *pattern) {
    chDbgCheck(reader != NULL);

    reader->pattern = pattern;
    reader->step_index = 0U;
    reader->step_has_playable_voice = false;
    reader->step_has_automation = false;

    if (pattern != NULL) {
        reader->last_generation = pattern->generation;
        _seq_engine_reader_refresh_flags(reader);
    } else {
        reader->last_generation.value = 0U;
    }
}

static void _seq_engine_reader_refresh_flags(seq_engine_reader_t *reader) {
    if (reader == NULL) {
        return;
    }

    reader->step_has_playable_voice = false;
    reader->step_has_automation = false;

    if ((reader->pattern == NULL) || (reader->step_index >= SEQ_MODEL_STEPS_PER_PATTERN)) {
        return;
    }

    const seq_model_step_t *step = &reader->pattern->steps[reader->step_index];
    const bool has_voice = seq_model_step_has_playable_voice(step);
    reader->step_has_playable_voice = has_voice;
    reader->step_has_automation = seq_model_step_is_automation_only(step);
}

static void _seq_engine_player_init(seq_engine_player_t *player) {
    chDbgCheck(player != NULL);

    player->thread = NULL;
    player->running = false;
}

static THD_FUNCTION(_seq_engine_player_thread, arg) {
    seq_engine_t *engine = (seq_engine_t *)arg;

#if CH_CFG_USE_REGISTRY
    chRegSetThreadName("seq_player");
#endif

    for (;;) {
        seq_engine_event_t event;

        chMtxLock(&engine->scheduler_lock);
        const bool has_event = seq_engine_scheduler_peek(&engine->scheduler, &event);
        const bool running = engine->player.running;
        if (!running && !has_event) {
            chMtxUnlock(&engine->scheduler_lock);
            break;
        }

        if (!has_event) {
            chMtxUnlock(&engine->scheduler_lock);
            chBSemWaitTimeout(&engine->player_sem, TIME_INFINITE);
            continue;
        }

        const systime_t now = chVTGetSystemTimeX();
        if (event.scheduled_time > now) {
            const systime_t wait = event.scheduled_time - now;
            chMtxUnlock(&engine->scheduler_lock);
            chBSemWaitTimeout(&engine->player_sem, wait);
            continue;
        }

        (void)seq_engine_scheduler_pop(&engine->scheduler, &event);
        chMtxUnlock(&engine->scheduler_lock);

        _seq_engine_dispatch_event(engine, &event);
    }
}

static void _seq_engine_dispatch_event(seq_engine_t *engine, const seq_engine_event_t *event) {
    if ((engine == NULL) || (event == NULL)) {
        return;
    }

    switch (event->type) {
    case SEQ_ENGINE_EVENT_NOTE_ON: {
        const seq_engine_note_on_t *note_on = &event->data.note_on;
        if (note_on->voice < SEQ_MODEL_VOICES_PER_STEP) {
            engine->voice_active[note_on->voice] = true;
            engine->voice_note[note_on->voice] = note_on->note;
        }
        if (engine->config.callbacks.note_on != NULL) {
            engine->config.callbacks.note_on(note_on, event->scheduled_time);
        }
        break;
    }
    case SEQ_ENGINE_EVENT_NOTE_OFF: {
        const seq_engine_note_off_t *note_off = &event->data.note_off;
        if (note_off->voice < SEQ_MODEL_VOICES_PER_STEP) {
            engine->voice_active[note_off->voice] = false;
            engine->voice_note[note_off->voice] = note_off->note;
        }
        if (engine->config.callbacks.note_off != NULL) {
            engine->config.callbacks.note_off(note_off, event->scheduled_time);
        }
        break;
    }
    case SEQ_ENGINE_EVENT_PLOCK: {
        if (engine->config.callbacks.plock != NULL) {
            engine->config.callbacks.plock(&event->data.plock, event->scheduled_time);
        }
        break;
    }
    default:
        break;
    }
}

static void _seq_engine_signal_player(seq_engine_t *engine) {
    chBSemSignal(&engine->player_sem);
}

static bool _seq_engine_schedule_event(seq_engine_t *engine, const seq_engine_event_t *event) {
    bool queued = false;

    chMtxLock(&engine->scheduler_lock);
    queued = seq_engine_scheduler_push(&engine->scheduler, event);
    chMtxUnlock(&engine->scheduler_lock);

    if (queued) {
        _seq_engine_signal_player(engine);
    }

    return queued;
}

static void _seq_engine_all_notes_off(seq_engine_t *engine) {
    if (engine == NULL) {
        return;
    }

    const seq_engine_note_off_cb_t cb = engine->config.callbacks.note_off;
    if (cb == NULL) {
        return;
    }

    const systime_t now = chVTGetSystemTimeX();
    for (uint8_t v = 0U; v < SEQ_MODEL_VOICES_PER_STEP; ++v) {
        if (!engine->voice_active[v]) {
            continue;
        }
        seq_engine_note_off_t off = {
            .voice = v,
            .note = engine->voice_note[v]
        };
        cb(&off, now);
        engine->voice_active[v] = false;
    }
}

static void _seq_engine_reset_voice_state(seq_engine_t *engine) {
    if (engine == NULL) {
        return;
    }

    memset(engine->voice_active, 0, sizeof(engine->voice_active));
    memset(engine->voice_note, 0, sizeof(engine->voice_note));
}

static bool _seq_engine_is_track_muted(const seq_engine_t *engine, uint8_t track) {
    if ((engine == NULL) || (engine->config.is_track_muted == NULL)) {
        return false;
    }
    return engine->config.is_track_muted(track);
}

static uint8_t _seq_engine_apply_scale(uint8_t note, const seq_model_scale_config_t *scale) {
    if ((scale == NULL) || !scale->enabled) {
        return note;
    }

    static const uint16_t masks[] = {
        0x0FFFU, /* Chromatic: unused when enabled flag false. */
        0x0AB5U, /* Major: 0,2,4,5,7,9,11 */
        0x05ADU, /* Minor (natural): 0,2,3,5,7,8,10 */
        0x06ADU, /* Dorian: 0,2,3,5,7,9,10 */
        0x06B5U  /* Mixolydian: 0,2,4,5,7,9,10 */
    };

    uint8_t mode = (uint8_t)scale->mode;
    if (mode >= (sizeof(masks) / sizeof(masks[0]))) {
        return note;
    }

    uint16_t mask = masks[mode];
    if (!scale->enabled || (mask == 0U)) {
        return note;
    }

    const uint8_t root = (uint8_t)(scale->root % 12U);
    int base = (int)note - (int)root;
    int octave = 0;
    if (base >= 0) {
        octave = base / 12;
    } else {
        octave = -((( -base) + 11) / 12);
    }
    int rel = base - (octave * 12);
    if (rel < 0) {
        rel += 12;
    }

    uint8_t pc = (uint8_t)(rel % 12);
    for (uint8_t attempts = 0U; attempts < 12U; ++attempts) {
        if ((mask >> pc) & 0x1U) {
            break;
        }
        pc = (uint8_t)((pc + 11U) % 12U);
    }

    int result = (int)root + (octave * 12) + pc;
    if (result < 0) {
        result = 0;
    } else if (result > 127) {
        result = 127;
    }

    return (uint8_t)result;
}

static int64_t _seq_engine_micro_to_delta(systime_t step_duration, int micro) {
    int64_t scaled = (int64_t)step_duration * (int64_t)micro;
    return scaled / SEQ_ENGINE_MICRO_DIVISOR;
}

static systime_t _seq_engine_saturate_time(int64_t value) {
    if (value <= 0) {
        return (systime_t)0;
    }

    int64_t max_time;
    if (sizeof(systime_t) == 2U) {
        max_time = 0xFFFFLL;
    } else if (sizeof(systime_t) == 4U) {
        max_time = 0xFFFFFFFFLL;
    } else {
        max_time = 0x7FFFFFFFFFFFFFFFLL;
    }

    if (value > max_time) {
        return (systime_t)max_time;
    }

    return (systime_t)value;
}

static void _seq_engine_schedule_plocks(seq_engine_t *engine,
                                        const seq_model_step_t *step,
                                        systime_t apply_time,
                                        systime_t restore_time) {
    if ((engine == NULL) || (step == NULL)) {
        return;
    }

    for (uint8_t i = 0U; i < step->plock_count; ++i) {
        const seq_model_plock_t *plock = &step->plocks[i];
        if (plock->domain != SEQ_MODEL_PLOCK_CART) {
            continue;
        }
        if (_seq_engine_is_track_muted(engine, plock->voice_index)) {
            continue;
        }

        seq_engine_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.type = SEQ_ENGINE_EVENT_PLOCK;
        ev.scheduled_time = apply_time;
        ev.data.plock.plock = *plock;
        ev.data.plock.action = SEQ_ENGINE_PLOCK_APPLY;
        BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_PLOCK", plock->parameter_id, plock->value, apply_time);
        _seq_engine_schedule_event(engine, &ev);

        memset(&ev, 0, sizeof(ev));
        ev.type = SEQ_ENGINE_EVENT_PLOCK;
        ev.scheduled_time = restore_time;
        ev.data.plock.plock = *plock;
        ev.data.plock.action = SEQ_ENGINE_PLOCK_RESTORE;
        BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_PLOCK_RESTORE", plock->parameter_id, plock->value, restore_time);
        _seq_engine_schedule_event(engine, &ev);
    }
}

static void _seq_engine_handle_step(seq_engine_t *engine,
                                    const seq_model_step_t *step,
                                    const clock_step_info_t *info,
                                    size_t step_index) {
    (void)step_index;

    if ((engine == NULL) || (step == NULL) || (info == NULL)) {
        return;
    }

    const bool automation_only = seq_model_step_is_automation_only(step);
    const bool has_voice = seq_model_step_has_playable_voice(step);
    const bool has_plock = seq_model_step_has_any_plock(step);

    if (!has_voice && !has_plock) {
        return;
    }

    const seq_model_pattern_t *pattern = engine->config.pattern;
    const seq_model_pattern_config_t *cfg = (pattern != NULL) ? &pattern->config : NULL;
    const seq_model_step_offsets_t *offsets = &step->offsets;

    const systime_t step_start = info->now;
    const systime_t step_duration = (info->step_st != 0U) ? info->step_st : 1U;
    const systime_t step_end = _seq_engine_saturate_time((int64_t)step_start + (int64_t)step_duration);

    seq_engine_event_t note_events[SEQ_MODEL_VOICES_PER_STEP * 2U];
    size_t note_event_count = 0U;
    systime_t earliest_on = step_start;
    bool any_voice_scheduled = false;

    if (!automation_only) {
        for (uint8_t voice_index = 0U; voice_index < SEQ_MODEL_VOICES_PER_STEP; ++voice_index) {
            const seq_model_voice_t *voice = &step->voices[voice_index];
            if ((voice->state != SEQ_MODEL_VOICE_ENABLED) || (voice->velocity == 0U)) {
                continue;
            }

            int32_t velocity = (int32_t)voice->velocity + (int32_t)offsets->velocity;
            if (velocity < 0) {
                velocity = 0;
            } else if (velocity > 127) {
                velocity = 127;
            }
            if (velocity == 0) {
                continue;
            }

            if (_seq_engine_is_track_muted(engine, voice_index)) {
                continue;
            }

            int32_t length_steps = (int32_t)voice->length + (int32_t)offsets->length;
            if (length_steps < 1) {
                length_steps = 1;
            } else if (length_steps > 64) {
                length_steps = 64;
            }

            int32_t note_value = (int32_t)voice->note + (int32_t)offsets->transpose;
            if (cfg != NULL) {
                note_value += cfg->transpose.global;
                if (voice_index < SEQ_MODEL_VOICES_PER_STEP) {
                    note_value += cfg->transpose.per_voice[voice_index];
                }
            }
            if (note_value < 0) {
                note_value = 0;
            } else if (note_value > 127) {
                note_value = 127;
            }
            uint8_t note = (uint8_t)note_value;

            if (cfg != NULL) {
                note = _seq_engine_apply_scale(note, &cfg->scale);
            }

            int32_t micro = (int32_t)voice->micro_offset + (int32_t)offsets->micro;
            if (micro < -SEQ_ENGINE_MICRO_MAX) {
                micro = -SEQ_ENGINE_MICRO_MAX;
            } else if (micro > SEQ_ENGINE_MICRO_MAX) {
                micro = SEQ_ENGINE_MICRO_MAX;
            }

            int64_t t_on_raw = (int64_t)step_start + _seq_engine_micro_to_delta(step_duration, micro);
            const systime_t note_on_time = _seq_engine_saturate_time(t_on_raw);
            int64_t t_off_raw = t_on_raw + ((int64_t)step_duration * (int64_t)length_steps);
            const systime_t note_off_time = _seq_engine_saturate_time(t_off_raw);

            seq_engine_event_t ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = SEQ_ENGINE_EVENT_NOTE_ON;
            ev.scheduled_time = note_on_time;
            ev.data.note_on.voice = voice_index;
            ev.data.note_on.note = note;
            ev.data.note_on.velocity = (uint8_t)velocity;
            note_events[note_event_count++] = ev;
            BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_NOTE_ON",
                                  (uint16_t)(((uint16_t)voice_index << 8) | note),
                                  velocity,
                                  note_on_time);

            memset(&ev, 0, sizeof(ev));
            ev.type = SEQ_ENGINE_EVENT_NOTE_OFF;
            ev.scheduled_time = note_off_time;
            ev.data.note_off.voice = voice_index;
            ev.data.note_off.note = note;
            note_events[note_event_count++] = ev;
            BRICK_DEBUG_PLOCK_LOG("ENGINE_SCHED_NOTE_OFF",
                                  (uint16_t)(((uint16_t)voice_index << 8) | note),
                                  0,
                                  note_off_time);

            any_voice_scheduled = true;
            if (note_on_time < earliest_on) {
                earliest_on = note_on_time;
            }
        }
    }

    if (has_plock) {
        systime_t dispatch_time = step_start;
        if (any_voice_scheduled && (info->tick_st != 0U)) {
            const systime_t half_tick = info->tick_st / 2U;
            if (earliest_on > half_tick) {
                const systime_t candidate = earliest_on - half_tick;
                if (candidate > dispatch_time) {
                    dispatch_time = candidate;
                }
            }
        }
        _seq_engine_schedule_plocks(engine, step, dispatch_time, step_end);
    }

    // --- FIX: ordonner les évènements pour éviter qu'un NOTE OFF bloque les autres voix ---
    if (note_event_count > 1U) {
        for (size_t i = 1U; i < note_event_count; ++i) {
            seq_engine_event_t tmp = note_events[i];
            size_t j = i;
            while ((j > 0U) && (note_events[j - 1U].scheduled_time > tmp.scheduled_time)) {
                note_events[j] = note_events[j - 1U];
                --j;
            }
            note_events[j] = tmp;
        }
    }

    for (size_t i = 0U; i < note_event_count; ++i) {
        _seq_engine_schedule_event(engine, &note_events[i]);
    }
}
