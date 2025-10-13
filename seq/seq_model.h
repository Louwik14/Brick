/**
 * @file seq_model.h
 * @brief Pure data model for the Brick sequencer (pattern, steps, P-Locks).
 * @ingroup seq_model
 */
#ifndef BRICK_SEQ_MODEL_H
#define BRICK_SEQ_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup seq_model Sequencer Data Model
 *  @brief Immutable-friendly pattern representation used by the engine.
 *  @{ */

/** Number of DSP carts handled simultaneously. */
#define SEQ_MODEL_CART_COUNT 4U

/** Logical voices per DSP cart. */
#define SEQ_MODEL_VOICES_PER_CART 4U

/** Maximum number of voices managed by the sequencer. */
#define SEQ_MODEL_VOICE_COUNT (SEQ_MODEL_CART_COUNT * SEQ_MODEL_VOICES_PER_CART)

/** Default base note (C4) applied when clearing a step. */
#define SEQ_MODEL_DEFAULT_NOTE 60

/** Default velocity applied when clearing a step. */
#define SEQ_MODEL_DEFAULT_VELOCITY 100

/** Default gate length (1 step). */
#define SEQ_MODEL_DEFAULT_LENGTH 1

/** Default micro-timing offset (on grid). */
#define SEQ_MODEL_DEFAULT_MICRO 0

/** Maximum number of steps per pattern. */
#define SEQ_MODEL_STEP_COUNT 64U

/** Maximum allowed micro-timing offset in ticks (signed). */
#define SEQ_MODEL_MICRO_OFFSET_RANGE 96

/** Bit-mask storing which parameters are locked for a step. */
typedef uint8_t seq_plock_mask_t;

/** Identifiers for per-step parameters that can be P-Locked. */
typedef enum {
    SEQ_PARAM_NOTE = 0,      /**< MIDI note number override */
    SEQ_PARAM_VELOCITY,      /**< Velocity override */
    SEQ_PARAM_LENGTH,        /**< Gate length override (in steps) */
    SEQ_PARAM_MICRO_TIMING,  /**< Micro timing offset */
    SEQ_PARAM_COUNT
} seq_param_id_t;

/** Global offsets applied as post-process after per-step values. */
typedef struct {
    int16_t transpose;   /**< Global transpose applied to all notes. */
    int16_t velocity;    /**< Velocity offset (clamped 0..127). */
    int16_t length;      /**< Gate length offset in steps. */
    int16_t micro_timing;/**< Micro timing offset in ticks. */
} seq_offsets_t;

/** Representation of a single sequencer step. */
typedef struct {
    bool            active;      /**< True if the step holds at least one voice. */
    uint8_t         note;        /**< Base MIDI note (0..127). */
    uint8_t         velocity;    /**< Base velocity (0..127). */
    uint8_t         length;      /**< Gate length in steps (minimum 1). */
    int8_t          micro_timing;/**< Micro timing in ticks relative to clock. */
    seq_plock_mask_t plock_mask; /**< Bit-mask of locked parameters. */
    int16_t         params[SEQ_PARAM_COUNT]; /**< Locked parameter values. */
} seq_step_t;

/** One polyphonic track (voice) of the sequencer. */
typedef struct {
    seq_step_t steps[SEQ_MODEL_STEP_COUNT]; /**< Per-step state. */
    uint16_t   length;                     /**< Number of active steps (1..64). */
} seq_track_t;

/** Complete pattern shared between the engine and runtime. */
typedef struct {
    seq_track_t  voices[SEQ_MODEL_VOICE_COUNT]; /**< Four independent voices. */
    seq_offsets_t offsets;                      /**< Global offsets. */
    uint32_t     generation;                    /**< Monotonic generation counter. */
} seq_pattern_t;

/** Initialise the pattern with sane defaults (C4 steps, all muted). */
void seq_model_init(seq_pattern_t *pattern);

/** Clear all steps but preserve global offsets. */
void seq_model_clear(seq_pattern_t *pattern);

/** Set the looping length of a voice (clamped to 1..64). */
void seq_model_voice_set_length(seq_pattern_t *pattern, uint8_t voice, uint16_t length);

/** Retrieve the looping length of a voice (defaults to 64 on invalid input). */
uint16_t seq_model_voice_length(const seq_pattern_t *pattern, uint8_t voice);

/** Toggle a step on/off for the provided voice. */
void seq_model_toggle_step(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx);

/** Force the active flag for a given step and voice. */
void seq_model_set_step_active(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx, bool active);

/** Query whether a step is active. */
bool seq_model_step_is_active(const seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx);

/** Write a step parameter (note/velocity/length/micro) and optionally set the P-Lock flag. */
void seq_model_set_step_param(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx,
                              seq_param_id_t param, int16_t value, bool enable_plock);

/** Read a step parameter value and expose whether it is currently P-Locked. */
int16_t seq_model_step_param(const seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx,
                             seq_param_id_t param, bool *is_plocked);

/** Remove all P-Lock flags and values for the selected step. */
void seq_model_clear_step_params(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx);

/**
 * @brief Clear a step completely (note + P-Locks) and restore defaults.
 * @details
 *  Utilisé pour le « quick clear » : supprime l’éventuel P-Lock résiduel et
 *  restaure les valeurs live (note C4, vélocité 100, longueur 1, micro=0) afin
 *  que la réactivation ultérieure reparte d’un état propre.
 */
void seq_model_step_clear_all(seq_pattern_t *pattern, uint8_t voice, uint16_t step_idx);

/** Update the pattern-wide offsets used by the engine. */
void seq_model_set_offsets(seq_pattern_t *pattern, const seq_offsets_t *offsets);

/** Read-only access to the offsets structure. */
const seq_offsets_t *seq_model_get_offsets(const seq_pattern_t *pattern);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BRICK_SEQ_MODEL_H */
