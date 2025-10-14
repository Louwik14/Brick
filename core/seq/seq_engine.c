/**
 * @file seq_engine.c
 * @brief Sequencer engine skeleton implementation.
 */

#include "seq_engine.h"

#include <string.h>

#include "clock_manager.h"

/** Convenience macro to wrap modulo operations on the pattern size. */
#define SEQ_ENGINE_WRAP_STEP(index) \
    (((index) >= SEQ_MODEL_STEPS_PER_PATTERN) ? ((index) - SEQ_MODEL_STEPS_PER_PATTERN) : (index))

static void _seq_engine_reader_init(seq_engine_reader_t *reader, const seq_model_pattern_t *pattern);
static void _seq_engine_reader_refresh_flags(seq_engine_reader_t *reader);
static void _seq_engine_player_init(seq_engine_player_t *player);
static void _seq_engine_on_clock_step(const clock_step_info_t *info);

static seq_engine_t *s_engine_clock_target = NULL;

void seq_engine_init(seq_engine_t *engine, const seq_engine_config_t *config) {
    chDbgCheck(engine != NULL);

    memset(engine, 0, sizeof(*engine));

    if (config != NULL) {
        engine->config = *config;
    }

    _seq_engine_reader_init(&engine->reader, engine->config.pattern);
    seq_engine_scheduler_clear(&engine->scheduler);
    _seq_engine_player_init(&engine->player);
    engine->clock_attached = false;
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

    s_engine_clock_target = engine;
    clock_manager_register_step_callback2(_seq_engine_on_clock_step);
    engine->player.running = true;
    engine->clock_attached = true;

    return MSG_OK;
}

void seq_engine_stop(seq_engine_t *engine) {
    chDbgCheck(engine != NULL);

    if (!engine->clock_attached) {
        return;
    }

    clock_manager_register_step_callback2(NULL);
    s_engine_clock_target = NULL;
    engine->player.running = false;
    engine->clock_attached = false;
}

void seq_engine_reset(seq_engine_t *engine) {
    chDbgCheck(engine != NULL);

    seq_engine_scheduler_clear(&engine->scheduler);
    _seq_engine_reader_init(&engine->reader, engine->config.pattern);
}

bool seq_engine_scheduler_push(seq_engine_scheduler_t *scheduler, const seq_engine_event_t *event) {
    chDbgCheck((scheduler != NULL) && (event != NULL));

    if (scheduler->count >= SEQ_ENGINE_SCHEDULER_CAPACITY) {
        return false;
    }

    size_t insert_index = (scheduler->head + scheduler->count) % SEQ_ENGINE_SCHEDULER_CAPACITY;
    scheduler->buffer[insert_index] = *event;
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

void seq_engine_scheduler_clear(seq_engine_scheduler_t *scheduler) {
    chDbgCheck(scheduler != NULL);

    scheduler->head = 0U;
    scheduler->count = 0U;
    memset(scheduler->buffer, 0, sizeof(scheduler->buffer));
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
    const bool has_voice = seq_model_step_has_active_voice(step);
    reader->step_has_playable_voice = has_voice;
    reader->step_has_automation = seq_model_step_is_automation_only(step);
}

static void _seq_engine_player_init(seq_engine_player_t *player) {
    chDbgCheck(player != NULL);

    player->thread = NULL;
    player->running = false;
}

static void _seq_engine_on_clock_step(const clock_step_info_t *info) {
    (void)info;

    if (s_engine_clock_target == NULL) {
        return;
    }

    seq_engine_reader_t *reader = &s_engine_clock_target->reader;
    if (reader->pattern == NULL) {
        return;
    }

    reader->step_index++;
    reader->step_index = SEQ_ENGINE_WRAP_STEP(reader->step_index);
    _seq_engine_reader_refresh_flags(reader);
}
