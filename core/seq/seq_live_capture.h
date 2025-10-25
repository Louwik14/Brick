#ifndef BRICK_CORE_SEQ_SEQ_LIVE_CAPTURE_H_
#define BRICK_CORE_SEQ_SEQ_LIVE_CAPTURE_H_

/**
 * @file seq_live_capture.h
 * @brief Live capture façade bridging UI events to the sequencer model.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clock_manager.h"
#include "seq_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration provided at initialisation.
 */
typedef struct {
    seq_model_track_t *track; /**< Optional initial track binding. */
} seq_live_capture_config_t;

/**
 * @brief Event type emitted by UI-facing inputs.
 */
typedef enum {
    SEQ_LIVE_CAPTURE_EVENT_NOTE_ON = 0,  /**< NOTE ON capture event. */
    SEQ_LIVE_CAPTURE_EVENT_NOTE_OFF      /**< NOTE OFF capture event. */
} seq_live_capture_event_type_t;

/**
 * @brief UI input translated into the capture façade.
 */
typedef struct {
    seq_live_capture_event_type_t type; /**< Event type. */
    uint8_t note;                       /**< MIDI note number. */
    uint8_t velocity;                   /**< MIDI velocity (0-127). */
    uint8_t voice_index;                /**< Suggested voice slot. */
    systime_t timestamp;                /**< Absolute timestamp of the event. */
} seq_live_capture_input_t;

/**
 * @brief Planned mutation returned to the caller.
 */
typedef struct {
    seq_live_capture_event_type_t type; /**< Event type echoed back. */
    size_t step_index;                  /**< Target step index inside the track. */
    int32_t step_delta;                 /**< Signed offset relative to the latest clock step. */
    uint8_t voice_index;                /**< Voice slot to affect. */
    uint8_t note;                       /**< MIDI note number. */
    uint8_t velocity;                   /**< MIDI velocity. */
    int8_t micro_offset;                /**< Planned micro-timing offset (-12..+12). */
    int8_t micro_adjust;                /**< Quantize correction compared to raw input. */
    bool quantized;                     /**< True if quantize altered the timing. */
    systime_t input_time;               /**< Raw timestamp of the incoming event. */
    systime_t scheduled_time;           /**< Timestamp at which the event should play. */
} seq_live_capture_plan_t;

/**
 * @brief Live capture façade context.
 */
typedef struct {
    seq_model_track_t *track;              /**< Active track reference. */
    seq_model_quantize_config_t quantize;    /**< Cached quantize configuration. */
    bool recording;                          /**< Recording flag. */
    bool clock_valid;                        /**< True once clock data has been provided. */
    systime_t clock_step_time;               /**< Timestamp of the latest 1/16 step boundary. */
    systime_t clock_step_duration;           /**< Duration of a 1/16 step. */
    systime_t clock_tick_duration;           /**< Duration of a single MIDI tick. */
    uint32_t clock_step_index;               /**< Absolute step index (monotonic). */
    size_t clock_track_step;               /**< Step index within the track. */
    struct {
        bool active;                         /**< True when a note-on has been captured. */
        size_t step_index;                   /**< Step index that received the note-on. */
        systime_t start_time;                /**< Scheduled playback time of the note-on. */
        systime_t start_time_raw;            /**< Raw timestamp captured at note-on. */
        systime_t step_duration;             /**< Step duration snapshot for the note. */
        uint8_t voice_slot;                  /**< Voice slot used to store the note. */
        uint8_t note;                        /**< MIDI note tied to the slot. */
    } voices[SEQ_MODEL_VOICES_PER_STEP];
} seq_live_capture_t;

/** Initialise the live capture context. */
void seq_live_capture_init(seq_live_capture_t *capture, const seq_live_capture_config_t *config);
/** Bind a track to the capture façade. */
void seq_live_capture_attach_track(seq_live_capture_t *capture, seq_model_track_t *track);
/** Override the quantize configuration used during capture. */
void seq_live_capture_override_quantize(seq_live_capture_t *capture, const seq_model_quantize_config_t *config);
/** Enable or disable live capture recording. */
void seq_live_capture_set_recording(seq_live_capture_t *capture, bool enabled);
/** Check whether live capture recording is enabled. */
bool seq_live_capture_is_recording(const seq_live_capture_t *capture);
/** Refresh the timing reference from the latest clock step. */
void seq_live_capture_update_clock(seq_live_capture_t *capture, const clock_step_info_t *info);
/** Plan an event using the current quantize/timing state. */
bool seq_live_capture_plan_event(seq_live_capture_t *capture,
                                 const seq_live_capture_input_t *input,
                                 seq_live_capture_plan_t *out_plan);
/** Commit a planned event into the bound track. */
bool seq_live_capture_commit_plan(seq_live_capture_t *capture,
                                  const seq_live_capture_plan_t *plan);

#ifdef __cplusplus
}
#endif

#endif  /* BRICK_CORE_SEQ_SEQ_LIVE_CAPTURE_H_ */
