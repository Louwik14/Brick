#ifndef BRICK_CORE_SEQ_SEQ_MODEL_H_
#define BRICK_CORE_SEQ_SEQ_MODEL_H_

/**
 * @file seq_model.h
 * @brief Brick sequencer data model definitions and helpers.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "seq_config.h"

#if SEQ_FEATURE_PLOCK_POOL
typedef struct __attribute__((packed)) {
    uint16_t offset;
    uint8_t count;
} seq_step_plock_ref_t;
_Static_assert(sizeof(seq_step_plock_ref_t) == 3, "pl_ref must be 3 bytes");
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of steps per track. */
#define SEQ_MODEL_STEPS_PER_TRACK   64U
/** Maximum number of voices per step. */
#define SEQ_MODEL_VOICES_PER_STEP     4U
/** Maximum number of parameter locks attached to a step. */
#define SEQ_MODEL_MAX_PLOCKS_PER_STEP 24U

/** Default velocity applied to the first voice when arming a step. */
#define SEQ_MODEL_DEFAULT_VELOCITY_PRIMARY   100U
/** Default velocity applied to secondary voices when arming a step. */
#define SEQ_MODEL_DEFAULT_VELOCITY_SECONDARY 0U

/** Sequencer generation counter used for dirty tracking. */
typedef struct {
    uint32_t value; /**< Monotonic counter incremented on every mutation. */
} seq_model_gen_t;

/** Voice enablement state. */
typedef uint8_t seq_model_voice_state_t;
enum {
    SEQ_MODEL_VOICE_DISABLED = 0U, /**< Voice is muted/off. */
    SEQ_MODEL_VOICE_ENABLED = 1U   /**< Voice produces note data. */
};

/** Types of parameter locks the model can store. */
typedef uint8_t seq_model_plock_domain_t;
enum {
    SEQ_MODEL_PLOCK_INTERNAL = 0U, /**< Internal engine parameters. */
    SEQ_MODEL_PLOCK_CART = 1U      /**< External cartridge parameter. */
};

/** Enumerates internal sequencer parameters that support parameter locks. */
typedef uint8_t seq_model_plock_internal_param_t;
enum {
    SEQ_MODEL_PLOCK_PARAM_NOTE = 0U,  /**< Note value override. */
    SEQ_MODEL_PLOCK_PARAM_VELOCITY,   /**< Velocity override. */
    SEQ_MODEL_PLOCK_PARAM_LENGTH,     /**< Step length override. */
    SEQ_MODEL_PLOCK_PARAM_MICRO,      /**< Micro-timing offset override. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_TR,  /**< Per-step transpose offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_VE,  /**< Per-step velocity offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_LE,  /**< Per-step length offset. */
    SEQ_MODEL_PLOCK_PARAM_GLOBAL_MI   /**< Per-step micro offset. */
};

/** Quantize grid resolution. */
typedef uint8_t seq_model_quantize_grid_t;
enum {
    SEQ_MODEL_QUANTIZE_1_4 = 0U,
    SEQ_MODEL_QUANTIZE_1_8,
    SEQ_MODEL_QUANTIZE_1_16,
    SEQ_MODEL_QUANTIZE_1_32,
    SEQ_MODEL_QUANTIZE_1_64
};

/** Available musical scales. */
typedef uint8_t seq_model_scale_mode_t;
enum {
    SEQ_MODEL_SCALE_CHROMATIC = 0U,
    SEQ_MODEL_SCALE_MAJOR,
    SEQ_MODEL_SCALE_MINOR,
    SEQ_MODEL_SCALE_DORIAN,
    SEQ_MODEL_SCALE_MIXOLYDIAN
};

/** Describes a single parameter lock. */
typedef struct {
    int16_t value;                   /**< Value payload (signed for offsets). */
    uint16_t parameter_id;           /**< Parameter identifier (cart domain). */
    seq_model_plock_domain_t domain; /**< Target domain. */
    uint8_t voice_index;             /**< Voice affected (0-3). */
    seq_model_plock_internal_param_t internal_param; /**< Internal parameter id. */
} seq_model_plock_t;

/** Per-voice information stored for each step. */
typedef struct {
    uint8_t note;                  /**< MIDI note number (0-127). */
    uint8_t velocity;              /**< MIDI velocity (0-127). */
    uint8_t length;                /**< Step length in sequencer ticks (1-64). */
    int8_t micro_offset;           /**< Micro-timing offset (-12..+12). */
    seq_model_voice_state_t state; /**< Active flag. */
} seq_model_voice_t;

/** Aggregate offsets applied to all voices on a step. */
typedef struct {
    int16_t velocity;  /**< Velocity offset (-127..+127). */
    int8_t transpose;  /**< Semitone transpose (-12..+12). */
    int8_t length;     /**< Length offset (-32..+32). */
    int8_t micro;      /**< Micro-timing offset (-12..+12). */
} seq_model_step_offsets_t;

/** Full step description including voices and parameter locks. */
typedef struct {
    uint8_t active : 1;     /**< True when at least one voice has velocity > 0. */
    uint8_t automation : 1; /**< True when the step is automation-only (no playable voices, has p-locks). */
    uint8_t reserved : 6;   /**< Reserved for future use. */
} seq_model_step_flags_t;

typedef struct seq_model_step_t {
    seq_model_voice_t voices[SEQ_MODEL_VOICES_PER_STEP]; /**< Voice data. */
#if !SEQ_FEATURE_PLOCK_POOL
    seq_model_plock_t plocks[SEQ_MODEL_MAX_PLOCKS_PER_STEP]; /**< Parameter locks. */
    uint8_t plock_count; /**< Number of active parameter locks. */
#endif
#if SEQ_FEATURE_PLOCK_POOL
    seq_step_plock_ref_t pl_ref;  /**< Reference into the packed p-lock pool. */
#endif
    seq_model_step_offsets_t offsets; /**< Per-step offsets. */
    seq_model_step_flags_t flags; /**< Cached step flags (playable / automation). */
} seq_model_step_t;

#if SEQ_FEATURE_PLOCK_POOL && defined(__GNUC__)
#pragma GCC poison seq_model_step_get_plock
#pragma GCC poison seq_model_step_add_plock
#pragma GCC poison seq_model_step_remove_plock
#pragma GCC poison seq_model_step_plock_count
#endif

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

/** Global track-wide configuration. */
typedef struct {
    seq_model_quantize_config_t quantize; /**< Quantize configuration. */
    seq_model_transpose_config_t transpose; /**< Transpose configuration. */
    seq_model_scale_config_t scale; /**< Scale configuration. */
} seq_model_track_config_t;

/** Track container used by the sequencer. */
typedef struct seq_model_track seq_model_track_t;

struct seq_model_track {
    seq_model_step_t steps[SEQ_MODEL_STEPS_PER_TRACK]; /**< Step list. */
    seq_model_track_config_t config; /**< Track-level configuration. */
    seq_model_gen_t generation; /**< Dirty tracking counter. */
};

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
/** Convert an empty step into a neutral quick-step shell. */
void seq_model_step_make_neutral(seq_model_step_t *step);
/** Convert a step into an automation-only placeholder (all voices muted). */
void seq_model_step_make_automation_only(seq_model_step_t *step);
/** Reset a full track to defaults. */
void seq_model_track_init(seq_model_track_t *track);

/** Retrieve a voice descriptor by index. */
const seq_model_voice_t *seq_model_step_get_voice(const seq_model_step_t *step, size_t voice_index);
/** Replace the voice descriptor at the provided index. */
bool seq_model_step_set_voice(seq_model_step_t *step, size_t voice_index, const seq_model_voice_t *voice);

/** Remove all parameter locks from a step. */
void seq_model_step_clear_plocks(seq_model_step_t *step);
#if !SEQ_FEATURE_PLOCK_POOL
/** Append a parameter lock to a step. */
bool seq_model_step_add_plock(seq_model_step_t *step, const seq_model_plock_t *plock);
/** Remove a parameter lock at the provided index. */
bool seq_model_step_remove_plock(seq_model_step_t *step, size_t index);
/** Retrieve a parameter lock by index. */
bool seq_model_step_get_plock(const seq_model_step_t *step, size_t index, seq_model_plock_t *out);
/** Return the number of parameter locks attached to a step. */
uint8_t seq_model_step_plock_count(const seq_model_step_t *step);

static inline uint8_t seq_model_step_legacy_pl_count(const seq_model_step_t *step) {
    return (step != NULL) ? step->plock_count : 0U;
}

static inline void seq_model_step_legacy_pl_set_count(seq_model_step_t *step, uint8_t count) {
    if (step != NULL) {
        step->plock_count = count;
    }
}

static inline seq_model_plock_t *seq_model_step_legacy_pl_storage(seq_model_step_t *step) {
    return (step != NULL) ? step->plocks : NULL;
}

static inline const seq_model_plock_t *seq_model_step_legacy_pl_storage_const(const seq_model_step_t *step) {
    return (step != NULL) ? step->plocks : NULL;
}

static inline int seq_model_step_legacy_pl_get(const seq_model_step_t *step,
                                               uint8_t index,
                                               uint8_t *out_id,
                                               uint8_t *out_value,
                                               uint8_t *out_flags) {
    (void)step;
    (void)index;
    (void)out_id;
    (void)out_value;
    (void)out_flags;
    return 0;
}
#endif

/** Assign the aggregate offsets for a step. */
void seq_model_step_set_offsets(seq_model_step_t *step, const seq_model_step_offsets_t *offsets);
/** Access the aggregate offsets for a step. */
const seq_model_step_offsets_t *seq_model_step_get_offsets(const seq_model_step_t *step);

/** Return true if at least one voice is enabled with a non-zero velocity. */
bool seq_model_step_has_playable_voice(const seq_model_step_t *step);
/** Return true if the step should be treated as automation-only. */
bool seq_model_step_is_automation_only(const seq_model_step_t *step);
/** Return true when the step exposes at least one parameter lock. */
bool seq_model_step_has_any_plock(const seq_model_step_t *step);
/** Return true when the step exposes at least one sequencer-domain parameter lock. */
bool seq_model_step_has_seq_plock(const seq_model_step_t *step);
/** Return true when the step exposes at least one cartridge-domain parameter lock. */
bool seq_model_step_has_cart_plock(const seq_model_step_t *step);
/** Recompute cached flags after mutating voices or parameter locks. */
void seq_model_step_recompute_flags(seq_model_step_t *step);

int seq_model_step_set_plocks_pooled(seq_model_step_t *step,
                                     const uint8_t *ids,
                                     const uint8_t *vals,
                                     const uint8_t *flags,
                                     uint8_t n);

#if SEQ_FEATURE_PLOCK_POOL
static inline uint8_t seq_model_step_pl_count_poolref(const seq_model_step_t *step) {
    return (step != NULL) ? step->pl_ref.count : 0U;
}

static inline uint16_t seq_model_step_pl_offset_poolref(const seq_model_step_t *step) {
    return (step != NULL) ? step->pl_ref.offset : 0U;
}
#endif

#if SEQ_MODEL_ENABLE_DEBUG_COUNTER
void seq_model_debug_reset_recompute_counter(void);
uint32_t seq_model_debug_get_recompute_counter(void);
#else
static inline void seq_model_debug_reset_recompute_counter(void) {}
static inline uint32_t seq_model_debug_get_recompute_counter(void) { return 0U; }
#endif

/** Flash-resident template used to initialise neutral sequencer steps. */
extern const seq_model_step_t k_seq_model_step_default;

/** Flash-resident default track configuration (quantize/transpose/scale). */
extern const seq_model_track_config_t k_seq_model_track_config_default;

/** Update the quantize configuration of a track. */
void seq_model_track_set_quantize(seq_model_track_t *track, const seq_model_quantize_config_t *config);
/** Update the transpose configuration of a track. */
void seq_model_track_set_transpose(seq_model_track_t *track, const seq_model_transpose_config_t *config);
/** Update the scale configuration of a track. */
void seq_model_track_set_scale(seq_model_track_t *track, const seq_model_scale_config_t *config);

#ifdef __cplusplus
}
#endif

#endif  /* BRICK_CORE_SEQ_SEQ_MODEL_H_ */
