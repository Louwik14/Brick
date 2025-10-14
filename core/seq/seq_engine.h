#ifndef BRICK_CORE_SEQ_SEQ_ENGINE_H_
#define BRICK_CORE_SEQ_SEQ_ENGINE_H_

/**
 * @file seq_engine.h
 * @brief Sequencer engine skeleton (reader, scheduler, player) definitions.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ch.h"

#include "seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of scheduled events retained by the scheduler. */
#define SEQ_ENGINE_SCHEDULER_CAPACITY 64U

/** Enumerates the type of events the scheduler can handle. */
typedef enum {
    SEQ_ENGINE_EVENT_NOTE_ON = 0,  /**< Dispatch a NOTE ON event. */
    SEQ_ENGINE_EVENT_NOTE_OFF,     /**< Dispatch a NOTE OFF event. */
    SEQ_ENGINE_EVENT_PLOCK         /**< Dispatch a parameter lock. */
} seq_engine_event_type_t;

/** NOTE ON payload describing a voice activation. */
typedef struct {
    uint8_t voice;       /**< Voice index associated with the NOTE ON. */
    uint8_t note;        /**< MIDI note number. */
    uint8_t velocity;    /**< MIDI velocity. */
} seq_engine_note_on_t;

/** NOTE OFF payload describing a voice release. */
typedef struct {
    uint8_t voice;       /**< Voice index associated with the NOTE OFF. */
    uint8_t note;        /**< MIDI note number. */
} seq_engine_note_off_t;

/** Parameter lock payload bridging to the model definition. */
typedef struct {
    seq_model_plock_t plock; /**< Parameter lock description. */
} seq_engine_plock_t;

/** Scheduled event description consumed by the player. */
typedef struct {
    seq_engine_event_type_t type; /**< Type of event. */
    systime_t scheduled_time;     /**< Absolute time when the event should fire. */
    union {
        seq_engine_note_on_t note_on;   /**< NOTE ON payload. */
        seq_engine_note_off_t note_off; /**< NOTE OFF payload. */
        seq_engine_plock_t plock;       /**< Parameter lock payload. */
    } data; /**< Event payload. */
} seq_engine_event_t;

/** FIFO queue used by the scheduler to hand events to the player. */
typedef struct {
    seq_engine_event_t buffer[SEQ_ENGINE_SCHEDULER_CAPACITY]; /**< Circular buffer. */
    size_t head;    /**< Index of the next event to pop. */
    size_t count;   /**< Number of events currently queued. */
} seq_engine_scheduler_t;

/** Reader state tracking the current pattern and dirty generation. */
typedef struct {
    const seq_model_pattern_t *pattern; /**< Active pattern. */
    size_t step_index;                  /**< Step index currently processed. */
    seq_model_gen_t last_generation;    /**< Snapshot for detecting edits. */
    bool step_has_playable_voice;       /**< True when the current step has at least one active voice. */
    bool step_has_automation;           /**< True when the current step only carries parameter locks. */
} seq_engine_reader_t;

/** Player execution context (thread stub for now). */
typedef struct {
    thread_t *thread; /**< Underlying worker thread (not started yet). */
    bool running;     /**< Running flag toggled by start/stop. */
} seq_engine_player_t;

/** Callback type for NOTE ON events. */
typedef msg_t (*seq_engine_note_on_cb_t)(const seq_engine_note_on_t *note_on,
                                         systime_t scheduled_time);
/** Callback type for NOTE OFF events. */
typedef msg_t (*seq_engine_note_off_cb_t)(const seq_engine_note_off_t *note_off,
                                          systime_t scheduled_time);
/** Callback type for parameter lock dispatch. */
typedef msg_t (*seq_engine_plock_cb_t)(const seq_engine_plock_t *plock,
                                       systime_t scheduled_time);

/** Bundle of callbacks invoked by the player. */
typedef struct {
    seq_engine_note_on_cb_t note_on; /**< NOTE ON dispatch callback. */
    seq_engine_note_off_cb_t note_off; /**< NOTE OFF dispatch callback. */
    seq_engine_plock_cb_t plock; /**< Parameter lock dispatch callback. */
} seq_engine_callbacks_t;

/** Configuration provided when initialising the engine. */
typedef struct {
    seq_model_pattern_t *pattern; /**< Initial pattern handled by the engine. */
    seq_engine_callbacks_t callbacks; /**< Dispatch callbacks. */
} seq_engine_config_t;

/** Aggregated engine context exposing reader, scheduler and player. */
typedef struct {
    seq_engine_reader_t reader;     /**< Pattern reader context. */
    seq_engine_scheduler_t scheduler; /**< Event scheduler queue. */
    seq_engine_player_t player;     /**< Player execution stub. */
    seq_engine_config_t config;     /**< Mutable configuration. */
    bool clock_attached;            /**< True when registered to the clock. */
} seq_engine_t;

void seq_engine_init(seq_engine_t *engine, const seq_engine_config_t *config);
void seq_engine_set_callbacks(seq_engine_t *engine, const seq_engine_callbacks_t *callbacks);
void seq_engine_attach_pattern(seq_engine_t *engine, seq_model_pattern_t *pattern);
msg_t seq_engine_start(seq_engine_t *engine);
void seq_engine_stop(seq_engine_t *engine);
void seq_engine_reset(seq_engine_t *engine);
bool seq_engine_scheduler_push(seq_engine_scheduler_t *scheduler, const seq_engine_event_t *event);
bool seq_engine_scheduler_pop(seq_engine_scheduler_t *scheduler, seq_engine_event_t *out_event);
void seq_engine_scheduler_clear(seq_engine_scheduler_t *scheduler);

#ifdef __cplusplus
}
#endif

#endif  /* BRICK_CORE_SEQ_SEQ_ENGINE_H_ */
