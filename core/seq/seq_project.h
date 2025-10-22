#ifndef BRICK_CORE_SEQ_SEQ_PROJECT_H_
#define BRICK_CORE_SEQ_SEQ_PROJECT_H_

/**
 * @file seq_project.h
 * @brief Sequencer multi-track project container helpers.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "seq_model.h"
#include "board/board_flash.h"
#include "core/brick_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of logical tracks a project can expose. */
#define SEQ_PROJECT_MAX_TRACKS 16U

/** Number of banks stored by a project. */
#define SEQ_PROJECT_BANK_COUNT 16U

/** Number of patterns per bank. */
#define SEQ_PROJECT_PATTERNS_PER_BANK 16U

/** Maximum number of bytes reserved per pattern in flash. */
#define SEQ_PROJECT_PATTERN_STORAGE_MAX 3968U

/** Serialized pattern version emitted by the firmware. */
#if BRICK_EXPERIMENTAL_PATTERN_CODEC_V2
#define SEQ_PROJECT_PATTERN_VERSION   2U
#else
#define SEQ_PROJECT_PATTERN_VERSION   1U
#endif

/** Size of a project slot in external flash. */
#define SEQ_PROJECT_FLASH_SLOT_SIZE (1024U * 1024U)

/** Maximum number of persistent projects. */
#define SEQ_PROJECT_MAX_PROJECTS (BOARD_FLASH_CAPACITY_BYTES / SEQ_PROJECT_FLASH_SLOT_SIZE)

/** Maximum length for project names. */
#define SEQ_PROJECT_NAME_MAX 24U

/** Maximum length for pattern names. */
#define SEQ_PROJECT_PATTERN_NAME_MAX 16U

/** Decode policy for standalone pattern payloads. */
typedef enum {
    SEQ_PROJECT_TRACK_DECODE_FULL = 0,
    SEQ_PROJECT_TRACK_DECODE_DROP_CART,
    SEQ_PROJECT_TRACK_DECODE_ABSENT
} seq_project_track_decode_policy_t;

/** Flags attached to a cart reference. */
typedef uint8_t seq_project_cart_flags_t;
enum {
    SEQ_PROJECT_CART_FLAG_NONE  = 0U,
    SEQ_PROJECT_CART_FLAG_MUTED = 1U << 0
};

/** Capabilities advertised by a cart reference. */
typedef uint16_t seq_project_cart_caps_t;
enum {
    SEQ_PROJECT_CART_CAP_NONE = 0U
};

/** Persistent reference describing how a track binds to a cartridge. */
typedef struct {
    uint32_t                cart_id;      /**< Unique cartridge identifier. */
    uint8_t                 slot_id;      /**< Physical slot the cart was saved from. */
    seq_project_cart_caps_t capabilities; /**< Capability bitmask. */
    seq_project_cart_flags_t flags;       /**< Runtime flags (muted, etc.). */
    uint8_t                 reserved;     /**< Reserved for alignment/future use. */
} seq_project_cart_ref_t;

/** Runtime track binding stored by a project. */
typedef struct {
    seq_model_track_t *track; /**< Mutable track assigned to the project slot. */
    seq_project_cart_ref_t cart;  /**< Cartridge metadata for persistence. */
} seq_project_track_t;

/** Descriptor of a track stored inside a persistent pattern slot. */
typedef struct {
    seq_project_cart_ref_t cart; /**< Cartridge reference for the saved track. */
    uint8_t                valid;/**< True when this entry contains data. */
    uint8_t                reserved[3]; /**< Alignment for future flags. */
} seq_project_track_desc_t;

/** Metadata associated with a pattern slot. */
typedef struct {
    char                          name[SEQ_PROJECT_PATTERN_NAME_MAX]; /**< Optional label. */
    uint8_t                       version;    /**< On-disk version. */
    uint8_t                       track_count;/**< Number of tracks stored. */
    uint16_t                      reserved;   /**< Alignment. */
    uint32_t                      storage_offset; /**< Absolute offset inside flash slot. */
    uint32_t                      storage_length; /**< Serialized payload length. */
    seq_project_track_desc_t tracks[SEQ_PROJECT_MAX_TRACKS]; /**< Track descriptors. */
} seq_project_pattern_desc_t;

/** Metadata for a bank (collection of 16 patterns). */
typedef struct {
    seq_project_pattern_desc_t patterns[SEQ_PROJECT_PATTERNS_PER_BANK];
} seq_project_bank_t;

/** Sequencer project aggregating multiple banks and runtime tracks. */
typedef struct seq_project seq_project_t;

struct seq_project {
    seq_project_bank_t banks[SEQ_PROJECT_BANK_COUNT]; /**< Persistent metadata. */
    seq_project_track_t tracks[SEQ_PROJECT_MAX_TRACKS]; /**< Runtime track bindings. */
    uint8_t track_count;       /**< Highest contiguous track index bound. */
    uint8_t active_track;      /**< Currently selected track index. */
    uint8_t active_bank;       /**< Currently selected bank. */
    uint8_t active_pattern;    /**< Currently selected pattern inside the bank. */
    uint8_t project_index;     /**< Active persistent project slot. */
    seq_model_gen_t generation;/**< Generation bumped on topology changes. */
    uint32_t tempo;            /**< Project tempo snapshot. */
    char name[SEQ_PROJECT_NAME_MAX]; /**< Project label. */
};

void seq_project_init(seq_project_t *project);
bool seq_project_assign_track(seq_project_t *project, uint8_t track_index, seq_model_track_t *track);
seq_model_track_t *seq_project_get_track(seq_project_t *project, uint8_t track_index);
const seq_model_track_t *seq_project_get_track_const(const seq_project_t *project, uint8_t track_index);
bool seq_project_set_active_track(seq_project_t *project, uint8_t track_index);
uint8_t seq_project_get_active_track_index(const seq_project_t *project);
seq_model_track_t *seq_project_get_active_track(seq_project_t *project);
const seq_model_track_t *seq_project_get_active_track_const(const seq_project_t *project);
uint8_t seq_project_get_track_count(const seq_project_t *project);
void seq_project_clear_track(seq_project_t *project, uint8_t track_index);
void seq_project_bump_generation(seq_project_t *project);
const seq_model_gen_t *seq_project_get_generation(const seq_project_t *project);
void seq_project_set_track_cart(seq_project_t *project, uint8_t track_index, const seq_project_cart_ref_t *cart);
const seq_project_cart_ref_t *seq_project_get_track_cart(const seq_project_t *project, uint8_t track_index);
bool seq_project_set_active_slot(seq_project_t *project, uint8_t bank, uint8_t pattern);
uint8_t seq_project_get_active_bank(const seq_project_t *project);
uint8_t seq_project_get_active_pattern_index(const seq_project_t *project);
seq_project_pattern_desc_t *seq_project_get_pattern_descriptor(seq_project_t *project, uint8_t bank, uint8_t pattern);
const seq_project_pattern_desc_t *seq_project_get_pattern_descriptor_const(const seq_project_t *project, uint8_t bank, uint8_t pattern);
bool seq_project_save(uint8_t project_index);
bool seq_project_load(uint8_t project_index);
bool seq_pattern_save(uint8_t bank, uint8_t pattern);
bool seq_pattern_load(uint8_t bank, uint8_t pattern);

bool seq_project_track_steps_encode(const seq_model_track_t *track,
                                      uint8_t *buffer,
                                      size_t buffer_size,
                                      size_t *written);

bool seq_project_track_steps_decode(seq_model_track_t *track,
                                      const uint8_t *buffer,
                                      size_t buffer_size,
                                      uint8_t version,
                                      seq_project_track_decode_policy_t policy);

#ifdef __cplusplus
}
#endif

#endif /* BRICK_CORE_SEQ_SEQ_PROJECT_H_ */
