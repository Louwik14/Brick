/**
 * @file seq_engine.h
 * @brief Sequencer playback engine driven by the global clock.
 * @ingroup seq_engine
 */
#ifndef BRICK_SEQ_ENGINE_H
#define BRICK_SEQ_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

#include "seq_model.h"
#include "seq_runtime.h"
#include "clock_manager.h"
#include "midi.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup seq_engine Sequencer Playback Engine
 *  @brief Clock-driven player, MIDI emitter and snapshot publisher.
 *  @{ */

typedef struct {
    midi_dest_t dest;                                  /**< Output destination (UART/USB/BOTH). */
    uint8_t     midi_channel[SEQ_MODEL_VOICE_COUNT];   /**< Per-voice MIDI channels (1..16). */
} seq_engine_config_t;

/** Initialise the engine and subscribe to the global clock. */
void seq_engine_init(const seq_engine_config_t *cfg);

/** Notify the engine that transport has started (resets playhead and gates). */
void seq_engine_transport_start(void);

/** Notify the engine that transport stopped (flushes pending notes). */
void seq_engine_transport_stop(void);

/** Toggle a step on/off for the selected voice. */
void seq_engine_toggle_step(uint8_t voice, uint32_t step_idx_abs);

/** Force the step into parameter-only mode (no gate). */
void seq_engine_set_step_param_only(uint8_t voice, uint32_t step_idx_abs, bool on);

/** Apply a delta on a parameter for all steps represented by the mask. */
void seq_engine_apply_plock_delta(seq_param_id_t param, int16_t delta, uint64_t step_mask);

/** Select which voice subsequent UI commands edit. */
void seq_engine_set_active_voice(uint8_t voice);

/** Apply a global offset (transpose/velocity/length/micro). */
void seq_engine_set_global_offset(seq_param_id_t param, int16_t value);

/** Adjust the loop length for a specific voice. */
void seq_engine_set_voice_length(uint8_t voice, uint16_t length);

/** Update the MIDI channel associated with a voice (1..16). */
void seq_engine_set_voice_channel(uint8_t voice, uint8_t channel);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_ENGINE_H */
