#ifndef BRICK_CORE_SEQ_SEQ_MODEL_H_
#define BRICK_CORE_SEQ_SEQ_MODEL_H_

/**
 * @file seq_model.h
 * @brief Brick sequencer data model definitions and helpers.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of steps per pattern. */
#define SEQ_MODEL_STEPS_PER_PATTERN   64U
/** Maximum number of voices per step. */
#define SEQ_MODEL_VOICES_PER_STEP     4U
/** Maximum number of parameter locks attached to a step. */
#define SEQ_MODEL_MAX_PLOCKS_PER_STEP 24U

/** Default note assigned to voices when initialising. */
#define SEQ_MODEL_DEFAULT_NOTE               60U
/** Default velocity applied to the first voice when arming a step. */
#define SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY   100U
/** Default velocity applied to secondary voices when arming a step. */
#define SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY 0U

/** Sequencer generation counter used for dirty tracking. */
typedef struct {
    uint32_t value; /**< Monotonic counter incremented on every mutation. */
} seq_model_gen_t;

/** Voice enablement state. */
typedef enum {
    SEQ_MODEL_VOICE_DISABLED = 0, /**< Voice is muted/off. */
    SEQ_MODEL_VOICE_ENABLED       /**< Voice produces note data. */
} seq_model_voice_state_t;

/** Types of parameter locks the model can store. */
typedef enum {
    SEQ_MODEL_PLOCK_INTERNAL = 0, /**< Internal engine parameters. */
    SEQ_MODEL_PLOCK_CART          /**< External cartridge parameter. */
} seq_model_plock_domain_t;

/** Enumerates internal sequencer parameters that support parameter locks. */
typedef enum {
    SEQ_MODEL_PLOCK_PARAM_NOTE = 0,  /**< Note value override. */
    SEQ_MODEL_PLOCK_PARAM_VELOCITY,  /**< Velocity override. */
    SEQ_MODEL_PLOCK_PARAM_LENGTH,    /**< Step length override. */
    SEQ_MODEL_PLOCK_PARAM_MICRO,     /**< Micro-timing offset override. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR, /**< Per-step transpose offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE, /**< Per-step velocity offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE, /**< Per-step length offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI  /**< Per-step micro offset. */
} seq_model_plock_internal_param_t;

/** Quantize grid resolution. */
typedef enum {
    SEQ_MODEL_QUANTIZE_1_4 = 0,
    SEQ_MODEL_QUANTIZE_1_8,
    SEQ_MODEL_QUANTIZE_1_16,
    SEQ_MODEL_QUANTIZE_1_32,
    SEQ_MODEL_QUANTIZE_1_64
} seq_model_quantize_grid_t;

/** Available musical scales. */
typedef enum {
    SEQ_MODEL_SCALE_CHROMATIC = 0,
    SEQ_MODEL_SCALE_MAJOR,
    SEQ_MODEL_SCALE_MINOR,
    SEQ_MODEL_SCALE_DORIAN,
    SEQ_MODEL_SCALE_MIXOLYDIAN
} seq_model_scale_mode_t;

/** Describes a single parameter lock. */
typedef struct {
    seq_model_plock_domain_t domain; /**< Target domain. */
    uint8_t voice_index;             /**< Voice affected (0-3). */
    uint16_t parameter_id;           /**< Parameter identifier (cart domain). */
    int16_t value;                   /**< Value payload (signed for offsets). */
    seq_model_plock_internal_param_t internal_param; /**< Internal parameter id. */
} seq_model_plock_t;

/** Per-voice information stored for each step. */
typedef struct {
    seq_model_voice_state_t state; /**< Active flag. */
    uint8_t note;                  /**< MIDI note number (0-127). */
    uint8_t velocity;              /**< MIDI velocity (0-127). */
    uint8_t length;                /**< Step length in sequencer ticks (1-64). */
    int8_t micro_offset;           /**< Micro-timing offset (-12..+12). */
} seq_model_voice_t;

/** Aggregate offsets applied to all voices on a step. */
typedef struct {
    int8_t transpose;  /**< Semitone transpose (-12..+12). */
    int16_t velocity;  /**< Velocity offset (-127..+127). */
    int8_t length;     /**< Length offset (-32..+32). */
    int8_t micro;      /**< Micro-timing offset (-12..+12). */
} seq_model_step_offsets_t;

/** Full step description including voices and parameter locks. */
typedef struct {
    seq_model_voice_t voices[SEQ_MODEL_VOICES_PER_STEP]; /**< Voice data. */
    seq_model_plock_t plocks[SEQ_MODEL_MAX_PLOCKS_PER_STEP]; /**< Parameter locks. */
    uint8_t plock_count; /**< Number of active parameter locks. */
    seq_model_step_offsets_t offsets; /**< Per-step offsets. */
} seq_model_step_t;

/** Quantization configuration applied during live capture. */
typedef struct {
    bool enabled;                            /**< Quantize switch. */
    seq_model_quantize_grid_t grid;          /**< Grid resolution. */
    uint8_t strength;                        /**< Strength (0-100%). */
} seq_model_quantize_config_t;

/** Transpose configuration for pattern playback. */
typedef struct {
    int8_t global;                                   /**< Global transpose (semitones). */
    int8_t per_voice[SEQ_MODEL_VOICES_PER_STEP];     /**< Per-voice transpose offsets. */
} seq_model_transpose_config_t;

/** Scale configuration clamping notes before scheduling. */
typedef struct {
    bool enabled;                  /**< Whether scale clamping is active. */
    uint8_t root;                  /**< Root note (0-11). */
    seq_model_scale_mode_t mode;   /**< Selected scale. */
} seq_model_scale_config_t;

/** Global pattern-wide configuration. */
typedef struct {
    seq_model_quantize_config_t quantize; /**< Quantize configuration. */
    seq_model_transpose_config_t transpose; /**< Transpose configuration. */
    seq_model_scale_config_t scale; /**< Scale configuration. */
} seq_model_pattern_config_t;

/** Pattern container used by the sequencer. */
typedef struct {
    seq_model_step_t steps[SEQ_MODEL_STEPS_PER_PATTERN]; /**< Step list. */
    seq_model_pattern_config_t config; /**< Pattern-level configuration. */
    seq_model_gen_t generation; /**< Dirty tracking counter. */
} seq_model_pattern_t;

/** Reset the generation counter to its initial value. */
void seq_model_gen_reset(seq_model_gen_t *gen);
/** Increment the generation counter after a mutation. */
void seq_model_gen_bump(seq_model_gen_t *gen);
/** Check whether two generation counters differ. */
bool seq_model_gen_has_changed(const seq_model_gen_t *lhs, const seq_model_gen_t *rhs);

/** Initialise a voice with Elektron-like defaults. */
void seq_model_voice_init(seq_model_voice_t *voice, bool primary);
/** Clear a step and restore default voices/offsets. */
void seq_model_step_init(seq_model_step_t *step);
/** Initialise a step using Elektron quick-step defaults for the provided note. */
void seq_model_step_init_default(seq_model_step_t *step, uint8_t note);
/** Reset a full pattern to defaults. */
void seq_model_pattern_init(seq_model_pattern_t *pattern);

/** Retrieve a voice descriptor by index. */
const seq_model_voice_t *seq_model_step_get_voice(const seq_model_step_t *step, size_t voice_index);
/** Replace the voice descriptor at the provided index. */
bool seq_model_step_set_voice(seq_model_step_t *step, size_t voice_index, const seq_model_voice_t *voice);

/** Append a parameter lock to a step. */
bool seq_model_step_add_plock(seq_model_step_t *step, const seq_model_plock_t *plock);
/** Remove all parameter locks from a step. */
void seq_model_step_clear_plocks(seq_model_step_t *step);
/** Remove a parameter lock at the provided index. */
bool seq_model_step_remove_plock(seq_model_step_t *step, size_t index);
/** Retrieve a parameter lock by index. */
bool seq_model_step_get_plock(const seq_model_step_t *step, size_t index, seq_model_plock_t *out);

/** Assign the aggregate offsets for a step. */
void seq_model_step_set_offsets(seq_model_step_t *step, const seq_model_step_offsets_t *offsets);
/** Access the aggregate offsets for a step. */
const seq_model_step_offsets_t *seq_model_step_get_offsets(const seq_model_step_t *step);

/** Return true if at least one voice is enabled with a non-zero velocity. */
bool seq_model_step_has_active_voice(const seq_model_step_t *step);
/** Return true when the step only carries parameter locks (no playable voice). */
bool seq_model_step_is_automation_only(const seq_model_step_t *step);
/** Convert a step into an automation-only placeholder (all voices muted). */
void seq_model_step_make_automate(seq_model_step_t *step);

/** Update the quantize configuration of a pattern. */
void seq_model_pattern_set_quantize(seq_model_pattern_t *pattern, const seq_model_quantize_config_t *config);
/** Update the transpose configuration of a pattern. */
void seq_model_pattern_set_transpose(seq_model_pattern_t *pattern, const seq_model_transpose_config_t *config);
/** Update the scale configuration of a pattern. */
void seq_model_pattern_set_scale(seq_model_pattern_t *pattern, const seq_model_scale_config_t *config);

#ifdef __cplusplus
}
#endif

#endif  /* BRICK_CORE_SEQ_SEQ_MODEL_H_ */
